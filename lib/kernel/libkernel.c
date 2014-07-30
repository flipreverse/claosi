#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <datamodel.h>
#include <resultset.h>
#include <query.h>
#include <api.h>

#define PROCFS_READ_BUFFER_SIZE		10

#ifndef VM_RESERVED
# define VM_RESERVED				(VM_DONTEXPAND | VM_DONTDUMP)
#endif

typedef struct DataModelMmap_t {
	unsigned long data;
	int reference;
} DataModelMmap_t;

static struct proc_dir_entry *procfsSlcDir = NULL;
static struct proc_dir_entry *procfsSlcDMFile = NULL;
static atomic_t datamodelFileRef;
static struct task_struct *queryExecThread = NULL;
static LIST_HEAD(queriesToExecList);
// Synchronize access to queriesToExecList
static DEFINE_SPINLOCK(listLock);
static DECLARE_WAIT_QUEUE_HEAD(waitQueue);
/*
 * Using list_empty() as a condition for wait_event() may lead to a race condition, especially on a smp system.
 * One processor executes enqueueQuery() while another one checks the condition '!list_empty(queriesToExecList)'.
 * So, we have to use a datatype which allows us atomically acesses *and* represents the lists status.
 * The waitingQueries variable holds the number of outstanding queries.
 */ 
static atomic_t waitingQueries;

void enqueueQuery(Query_t *query, Tupel_t *tuple) {
	QueryJob_t *job = NULL;
	unsigned long flags;

	/*
	 * Honestly, it is not necessary to check, if the execution thread is running.
	 * The module cannot be unloaded while there is at least one registered provider remaining.
	 * Therefore, it's impossible that neither the lock (listLock) nor the list (queriesToExecList)
	 * are NULL.
	 */
	spin_lock_irqsave(&listLock,flags);
	job = ALLOC(sizeof(QueryJob_t));
	if (job == NULL) {
		spin_unlock(&listLock);
		ERR_MSG("Cannot allocate memory for QueryJob_t\n");
		return;
	}
	job->query = query;
	job->tuple = tuple;
	// Enqueue it
	list_add_tail(&job->list,&queriesToExecList);
	atomic_inc(&waitingQueries);
	spin_unlock_irqrestore(&listLock,flags);

	DEBUG_MSG(2,"Enqueued query 0x%x with tuple %p for execution\n",job->query->queryID,job->tuple);
	// Notify the query execution about the outstanding query
	wake_up(&waitQueue);
}
EXPORT_SYMBOL(enqueueQuery);

static int generateObjectStatus(void *data) {
	QueryStatusJob_t *statusJob = (QueryStatusJob_t*)data;
	Tupel_t *curTuple = NULL;

	// The object may return a linked-list of Tuple_t
	curTuple = statusJob->statusFn();
	while (curTuple != NULL) {
		// Forward any query to the execution thread
		enqueueQuery(statusJob->query,curTuple);
		curTuple = curTuple->next;
	}
	
	FREE(statusJob);
	do_exit(0);
	return 0;
}

void startObjStatusThread(Query_t *query, generateStatus statusFn, unsigned long *__flags) {
	struct task_struct *objStatusThread = NULL;
	QueryStatusJob_t *statusJob = NULL;
	unsigned long flags = *__flags;

	// Allocate memory for the threads parameters
	statusJob = ALLOC(sizeof(QueryStatusJob_t));
	if (statusJob == NULL) {
		ERR_MSG("Cannot allocate memory for QueryStatusJob_t\n");
		return;
	}
	// Pass the query as well as the function pointer to the query
	statusJob->query = query;
	statusJob->statusFn = statusFn;
	/*
	 * startObjStatusThread just gets called from an atomic context (register{Provider,Query} --> addQuery --> startObjStatusThread).
	 * Hence, it is safe to release the slcLock.
	 * It mandatory to release the lock here, because the creation of a thread may trigger scheduling-related functions or even
	 * schedule() itself.
	 */
	RELEASE_WRITE_LOCK(slcLock);
	objStatusThread = kthread_create(generateObjectStatus,statusJob,"objStatusThread");
	if (IS_ERR(objStatusThread)) {
		FREE(statusJob);
		ACQUIRE_WRITE_LOCK(slcLock);
		*__flags = flags;
		ERR_MSG("Cannot start objStatusThread: %ld",PTR_ERR(objStatusThread));
		return;
	}
	wake_up_process(objStatusThread);
	// Re-acquire the lock and pass the flags upwards to the caller
	ACQUIRE_WRITE_LOCK(slcLock);
	*__flags = flags;
}

static enum hrtimer_restart hrtimerHandler(struct hrtimer *curTimer) {
	// Obtain the surrounding datatype
	QueryTimerJob_t *timerJob = container_of(curTimer,QueryTimerJob_t,timer);
	Source_t *src = (Source_t*)timerJob->dm->typeInfo;
	Tupel_t *tuple= NULL;
	unsigned long flags;

	DEBUG_MSG(2,"%s: Creating tuple\n",__FUNCTION__);
	// Only one timer at a time is allowed to access this source
	ACQUIRE_WRITE_LOCK(src->lock);
	tuple = src->callback();
	RELEASE_WRITE_LOCK(src->lock);
	if (tuple != NULL) {
		DEBUG_MSG(2,"Enqueue tuple\n");
		enqueueQuery(timerJob->query,tuple);
	}
	/*
	 * Restart the timer. Do *not* return HRTIMER_RESTART. It will force the kernel to *immediately* restart this timer and 
	 * will block at least one core. It will slow down the whole system.
	 */
	hrtimer_start(&timerJob->timer,ns_to_ktime(timerJob->period * NSEC_PER_MSEC),HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

void startSourceTimer(DataModelElement_t *dm, Query_t *query) {
	SourceStream_t *srcStream = (SourceStream_t*)query->root;
	QueryTimerJob_t *timerJob = NULL;
	// Allocate memory for the job-specific information; job-specific = (query,datamodel,period)
	timerJob = ALLOC(sizeof(QueryTimerJob_t));
	if (timerJob == NULL) {
		ERR_MSG("Cannot allocate QueryTimerJob_t\n");
		return;
	}

	DEBUG_MSG(2,"%s: Init hrtimer for node %s\n",__FUNCTION__,srcStream->st_name);
	// Setup the timer using timing information relative to the current clock which will be the monotonic one.
	hrtimer_init(&timerJob->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	timerJob->period = srcStream->period;
	timerJob->query = query;
	timerJob->dm = dm;
	timerJob->timer.function = &hrtimerHandler;
	srcStream->timerInfo = timerJob;

	DEBUG_MSG(2,"%s: Starting hrtimer for node %s. Will fire in %u ms.\n",__FUNCTION__,srcStream->st_name,srcStream->period);
	// Fire it up.... :-)
	hrtimer_start(&timerJob->timer,ns_to_ktime(srcStream->period * NSEC_PER_MSEC),HRTIMER_MODE_REL);
}

void stopSourceTimer(Query_t *query) {
	int ret = 0;
	SourceStream_t *srcStream = (SourceStream_t*)query->root;
	QueryTimerJob_t *timerJob = (QueryTimerJob_t*)srcStream->timerInfo;

	DEBUG_MSG(2,"%s: Canceling hrtimer for node %s...\n",__FUNCTION__,srcStream->st_name);
	// Cancel the timer. Blocks until the timer handler terminates.
	ret = hrtimer_cancel(&timerJob->timer);
	DEBUG_MSG(2,"%s: hrtimer for node %s canceled. Was active: %d\n",__FUNCTION__,srcStream->st_name,ret);
	// Free the timer information
	FREE(timerJob);
	srcStream->timerInfo = NULL;
}

static int queryExecutorWork(void *data) {
	QueryJob_t *cur = NULL;
	unsigned long flags;
	DEBUG_MSG(2,"Started execution thread\n");

	while (1) {
		DEBUG_MSG(3,"%s: Waiting for incoming queries...\n",__FUNCTION__);
		
		wait_event(waitQueue,kthread_should_stop() || atomic_read(&waitingQueries) > 0);

		while (atomic_read(&waitingQueries) > 0) {
			spin_lock(&listLock);
			// Dequeue the head and execute the query
			cur = list_first_entry(&queriesToExecList,QueryJob_t,list);
			list_del(&cur->list);
			atomic_dec(&waitingQueries);
			spin_unlock(&listLock);

			DEBUG_MSG(3,"%s: Executing query 0x%x with tuple %p\n",__FUNCTION__,cur->query->queryID,cur->tuple);
			// A queries execution just reads from the datamodel. No write lock is needed.
			ACQUIRE_READ_LOCK(slcLock);
			executeQuery(SLC_DATA_MODEL,cur->query,&cur->tuple);
			RELEASE_READ_LOCK(slcLock);
			FREE(cur);
		}
		if (kthread_should_stop()) {
			DEBUG_MSG(3,"%s: Were asked to terminate.\n",__FUNCTION__);
			break;
		}
	}

	return 0;
}

void datamodelFileMmapOpen(struct vm_area_struct *vma) {
	DataModelMmap_t *info = (DataModelMmap_t *)vma->vm_private_data;

	info->reference++;
	DEBUG_MSG(1,"reference (open) = %d\n",info->reference);
}

void datamodelFileMmapClose(struct vm_area_struct *vma) {
	DataModelMmap_t *info = (DataModelMmap_t *)vma->vm_private_data;

	info->reference--;
	DEBUG_MSG(1,"reference (close) = %d\n",info->reference);
}

/* nopage is called the first time a memory area is accessed which is not in memory,
 * it does the actual mapping between kernel and user space memory
 */
//struct page *mmap_nopage(struct vm_area_struct *vma, unsigned long address, int *type)	--changed
static int datamodelFileMmapFault(struct vm_area_struct *vma, struct vm_fault *vmf) {
	struct page *page;
	DataModelMmap_t *info;
	int offset = 0;
	void *ptr = NULL;

	/* the data is in vma->vm_private_data */
	info = (DataModelMmap_t *)vma->vm_private_data;
	if (!info->data) {
		ERR_MSG("No VMA private data!\n");
		return VM_FAULT_SIGBUS;
	}

	// Calculate the offset in bytes the user requests
	offset = (unsigned long)vmf->virtual_address - vma->vm_start;
	ptr = (void*)(info->data);
	// Resolve the virtual page address to a struct page
	page = virt_to_page(ptr);
	DEBUG_MSG(1,"page fault at 0x%p, offset = 0x%x, mapping page at 0x%p(0x%p)\n",vmf->virtual_address,offset,page_address(page),ptr);
	/* increment the reference count of this page */
	get_page(page);
	// Tell the memory management which page should be mapped at (vmf->virtual_address & PAGE_MASK)
	vmf->page = page;

	return 0;
}

struct vm_operations_struct datamodelfile_mmap_ops = {
	.open		=	datamodelFileMmapOpen,
	.close		=	datamodelFileMmapClose,
	.fault		=	datamodelFileMmapFault,
};

static int datamodelFileRead(struct file *fil, char __user *buffer, size_t buffer_length, loff_t *pos) {
	int ret = 0;

	if (*pos > 0) {
		return 0;
	}
	ret = snprintf(buffer,buffer_length, "0x%lx\n",(unsigned long)sharedMemoryBaseAddr);
	if (ret >= buffer_length) {
		// Not enough space to hold the whole string
		ret = buffer_length;
	} else {
		// Account for the null byte
		ret++;
	}
	*pos = ret;

	return ret;
}

static int datamodelFileMmap(struct file *filp, struct vm_area_struct *vma) {
	vma->vm_ops = &datamodelfile_mmap_ops;
	vma->vm_flags |= VM_RESERVED;
	/* assign the file private data to the vm private data */
	vma->vm_private_data = filp->private_data;

	DEBUG_MSG(1,"mapping datamodel space at 0x%p from 0x%lx to 0x%lx\n",filp->private_data,vma->vm_start,vma->vm_end);

	datamodelFileMmapOpen(vma);
	return 0;
}

static int datamodelFileClose(struct inode *inode, struct file *filp) {
	DataModelMmap_t *info = (DataModelMmap_t*)filp->private_data;

	kfree(info);
	filp->private_data = NULL;
	atomic_dec(&datamodelFileRef);

	return 0;
}

static int datamodelFileOpen(struct inode *inode, struct file *filp) {
	DataModelMmap_t *info = kmalloc(sizeof(DataModelMmap_t), GFP_KERNEL);

	if (atomic_read(&datamodelFileRef) >= 1) {
		return -EBUSY;
	}
	atomic_inc(&datamodelFileRef);
	/* obtain new memory */
	info->data = (unsigned long)sharedMemoryBaseAddr;
	/* assign this info struct to the file */
	filp->private_data = info;
	info->reference = 0;

	return 0;
}

static const struct file_operations proc_datamodel_operations = {
	.open		=	datamodelFileOpen,
	.read		=	datamodelFileRead,
	.release	=	datamodelFileClose,
	.mmap		=	datamodelFileMmap,
};

static int __init slc_init(void) {
	kuid_t fileUID;
	kgid_t fileGID;

	fileUID.val = 0;
	fileGID.val = 0;

	sharedMemoryBaseAddr = (void*)__get_free_pages(GFP_KERNEL | __GFP_COMP | __GFP_ZERO | __GFP_DMA,PAGE_ORDER);
	DEBUG_MSG(2,"Allocated %d pages of kernel memory for liballoc at 0x%p\n",NUM_PAGES,sharedMemoryBaseAddr);
	if (sharedMemoryBaseAddr == NULL) {
		return -ENOMEM;
	}

	procfsSlcDir = proc_mkdir(PROCFS_DIR_NAME, NULL);
	if (procfsSlcDir == NULL) {
		printk("Error: Could not initialize /proc/%s\n",PROCFS_DIR_NAME);
		return -ENOMEM;
	}
	proc_set_user(procfsSlcDir,fileUID,fileGID);

	procfsSlcDMFile = proc_create(PROCFS_DATAMODELFILE, 0777, procfsSlcDir,&proc_datamodel_operations);
	if (procfsSlcDMFile == NULL) {
		proc_remove(procfsSlcDir);
		printk("Error: Could not initialize /proc/%s/%s\n",PROCFS_DIR_NAME,PROCFS_DATAMODELFILE);
		return -ENOMEM;
	}
	proc_set_user(procfsSlcDMFile,fileUID,fileGID);
	proc_set_size(procfsSlcDMFile,7);

	atomic_set(&datamodelFileRef,0);

	if (initSLC() == -1) {
		return -1;
	}

	atomic_set(&waitingQueries,0);
	// Init ...
	queryExecThread = (struct task_struct*)kthread_create(queryExecutorWork,NULL,"queryExecThread");
	if (IS_ERR(queryExecThread)) {
		return PTR_ERR(queryExecThread);
	}
	// ... and start the query execution thread
	wake_up_process(queryExecThread);

	DEBUG_MSG(1,"Initialized SLC\n");
	return 0;
}

static void __exit slc_exit(void) {
	destroySLC();
	
	// Signal the query execution thread to terminate and wait for it
	kthread_stop(queryExecThread);
	queryExecThread = NULL;
	if (sharedMemoryBaseAddr != NULL) {
		free_pages((unsigned long)sharedMemoryBaseAddr,PAGE_ORDER);
	}

	proc_remove(procfsSlcDMFile);
	proc_remove(procfsSlcDir);

	DEBUG_MSG(1,"Destroyed SLC\n");
}

module_init(slc_init);
module_exit(slc_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
