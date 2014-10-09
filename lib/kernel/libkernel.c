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
#include <linux/sched/rt.h>
#include <datamodel.h>
#include <resultset.h>
#include <query.h>
#include <api.h>
#include <communication.h>

#define PROCFS_READ_BUFFER_SIZE		10
#define CALC_SLEEP_TIME
#undef CALC_SLEEP_TIME

#ifndef VM_RESERVED
# define VM_RESERVED				(VM_DONTEXPAND | VM_DONTDUMP)
#endif

typedef struct DataModelMmap_t {
	unsigned long data;
	int reference;
} DataModelMmap_t;

static struct proc_dir_entry *procfsSlcDir = NULL;
static struct proc_dir_entry *procfsSlcDMFile = NULL;
atomic_t communicationFileMmapRef;
static struct task_struct *queryExecThread = NULL;
static struct task_struct *commThread = NULL;
/*
 * An array of pointers to the NUM_PAGES instances of struct page.
 * Each represents on physical page.
 */
static struct page **sharedMemoryPages = NULL;
static LIST_HEAD(queriesToExecList);
// Synchronize access to queriesToExecList
static DEFINE_SPINLOCK(listLock);
/**
 * Account for the number of missed timers - have a look at hrtimerHandler()
 */
static atomic_t missedTimer;
static DECLARE_WAIT_QUEUE_HEAD(waitQueue);
/*
 * Using list_empty() as a condition for wait_event() may lead to a race condition, especially on a smp system.
 * One processor executes enqueueQuery() while another one checks the condition '!list_empty(queriesToExecList)'.
 * So, we have to use a datatype which allows us atomically acesses *and* represents the lists status.
 * The waitingQueries variable holds the number of outstanding queries.
 */ 
static atomic_t waitingQueries;
#ifdef CALC_SLEEP_TIME
/**
 * Number of successfull reads from the rx buffer
 */
static int successfullReads;
/**
 * Number of failed reads from the rx buffer
 */
static int failedReads;
#endif
/**
 * Amount of time to sleep after a failed read from the rx buffer
 */
static int sleepTime;
/**
 * Evaluate the maximum of waiting queries
 */
static unsigned long maxWaitingQueries;
static int useRTPrio = 0;
module_param(useRTPrio,int,S_IRUGO);

void enqueueQuery(Query_t *query, Tupel_t *tuple, int step) {
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
	job->step = step;
#ifdef EVALUATION
	job->tuple->timestamp2 = getCycles();
#endif
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
	Tupel_t *curTuple = NULL, *tempTuple = NULL;
	GenStream_t *stream = (GenStream_t*)statusJob->query->root;

	// The object may return a linked-list of Tuple_t
	curTuple = statusJob->statusFn(stream->selectors,stream->selectorsLen,NULL);
	while (curTuple != NULL) {
		tempTuple = curTuple->next;
		/*
		 * unlink curTuple from the list.
		 * Otherwise executeQuery() believes there are more tuple to process.
		 * Each tuple gets enqueued separately.
		 */
		curTuple->next = NULL;
		// Forward any query to the execution thread
		enqueueQuery(statusJob->query,curTuple,0);
		curTuple = tempTuple;
	}
	
	FREE(statusJob);
	do_exit(0);
	return 0;
}

void delPendingQuery(Query_t *query) {
	QueryJob_t *cur = NULL;
	struct list_head *pos = NULL, *next = NULL;
	
	spin_lock(&listLock);
	list_for_each_safe(pos, next, &queriesToExecList) {
		cur = list_entry(pos, QueryJob_t, list);
		if (cur->query == query) {
			DEBUG_MSG(1,"Found query 0x%lx. Removing it from list.\n",(unsigned long)cur->query);
			freeTupel(SLC_DATA_MODEL,cur->tuple);
			list_del(&cur->list);
			FREE(cur);
		}
	}
	spin_unlock(&listLock);
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
	GenStream_t *stream = (GenStream_t*)timerJob->query->root;
	Tupel_t *curTuple= NULL, *tempTuple = NULL;
	
	unsigned long flags;
	
	/*
	 * It is possible that the component deletes queries thus stopping a timer while a timer expires.
	 * Hence, the hrtimerHandler while stall at src->callback(), because the source wants to acquire the slcLock in order to create a tuple.
	 * After deleting the queries the lock will be release and the handler will acquire it.
	 * It is likeley that this timer was canceled. Therefore, the instance of TimerJob_t was freed. Further access to the recently freed memory location
	 * will lead to unpredictable behavior.
	 * To avoid this, each call to hrtimerHandler() will try to acquire the read lock. If it fails, the handler will abort.
	 * The delQuery() function will acquire a write lock.
	 */
	if (TRY_READ_LOCK(slcLock) == 0) {
		ERR_MSG("Cannot acquire read slc lock\n");
		atomic_inc(&missedTimer);
		return HRTIMER_RESTART;
	}
	DEBUG_MSG(2,"%s: Creating tuple\n",__FUNCTION__);
	// Only one timer at a time is allowed to access this source
	ACQUIRE_WRITE_LOCK(src->lock);
	curTuple = src->callback(stream->selectors,stream->selectorsLen,NULL);
	RELEASE_WRITE_LOCK(src->lock);
	while (curTuple != NULL) {
		tempTuple = curTuple->next;
		/*
		 * unlink curTuple from the list.
		 * Otherwise executeQuery() believes there are more tuple to process.
		 * Each tuple gets enqueued separately.
		 */
		curTuple->next = NULL;
		// Forward any query to the execution thread
		enqueueQuery(timerJob->query,curTuple,0);
		curTuple = tempTuple;
	}
	/*
	 * Restart the timer. Do *not* return HRTIMER_RESTART. It will force the kernel to *immediately* restart this timer and 
	 * will block at least one core. It will slow down the whole system.
	 */
	hrtimer_forward_now(&timerJob->timer,ns_to_ktime(timerJob->period * NSEC_PER_MSEC));
	RELEASE_READ_LOCK(slcLock);

	return HRTIMER_RESTART;
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

	if (timerJob == NULL) {
		ERR_MSG("No timer registered for query 0x%p\n",query);
		return;
	}
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
		
		wait_event_interruptible(waitQueue,kthread_should_stop() || atomic_read(&waitingQueries) > 0);
		while (atomic_read(&waitingQueries) > 0) {
			if (atomic_read(&waitingQueries) > maxWaitingQueries) {
				maxWaitingQueries = atomic_read(&waitingQueries);
			}
			read_lock(&slcLock);
			spin_lock_irqsave(&listLock,flags);
			// Dequeue the head and execute the query
			cur = list_first_entry(&queriesToExecList,QueryJob_t,list);
			list_del(&cur->list);
			atomic_dec(&waitingQueries);
			spin_unlock_irqrestore(&listLock,flags);

			DEBUG_MSG(3,"%s: Executing query 0x%x with tuple %p\n",__FUNCTION__,cur->query->queryID,cur->tuple);
			// A queries execution just reads from the datamodel. No write lock is needed.
			executeQuery(SLC_DATA_MODEL,cur->query,cur->tuple,cur->step);
			read_unlock(&slcLock);
			FREE(cur);
		}
		if (kthread_should_stop()) {
			DEBUG_MSG(3,"%s: Were asked to terminate.\n",__FUNCTION__);
			break;
		}
	}

	return 0;
}

#ifdef CALC_SLEEP_TIME
static int calcSleepTime(int sleepTime, int readSuccess) {
	int ret = sleepTime, temp = 0;

	if (readSuccess > 0) {
		successfullReads++;
		failedReads = 0;
	} else {
		successfullReads = 0;
		failedReads++;
	}
	if (successfullReads > SLEEP_THRESH_SUCESS) {
		temp = INIT_SLEEP_TIME - (successfullReads - SLEEP_THRESH_SUCESS) * SLEEP_STEP;
		ret = (temp < SLEEP_MIN ? SLEEP_MIN : temp);
	}
	if (failedReads > SLEEP_THRESH_FAILED) {
		temp = INIT_SLEEP_TIME + (failedReads - SLEEP_THRESH_FAILED) * SLEEP_STEP;
		ret = (temp > SLEEP_MAX ? SLEEP_MAX : temp);
	}
	ret = 100000;
	return ret;
}
#else
#define calcSleepTime(x,y)		(100000)
#endif

static int commThreadWork(void *data) {
	LayerMessage_t *msg = NULL;
	DataModelElement_t *dm = NULL;
	Query_t *query = NULL, *queryCopy = NULL;
	QueryContinue_t *queryCont = NULL;
	QueryID_t *queryID = NULL;
	Tupel_t *curTupleShm = NULL, *curTupleCopy = NULL, *headTupleCopy = NULL, *prevTupleCopy = NULL;
	int ret = 0;
	unsigned long flags;

	while (!kthread_should_stop()) {
		msg = ringBufferReadBegin(rxBuffer);
		if (msg == NULL) {
			usleep_range(sleepTime,sleepTime+500);
		} else {
			DEBUG_MSG(3,"Read msg with type 0x%x and addr 0x%p (rewritten addr = 0x%p)\n",msg->type,msg->addr,REWRITE_ADDR(msg->addr,sharedMemoryUserBase,sharedMemoryKernelBase));
			switch (msg->type) {
				case MSG_DM_ADD:
					dm = (DataModelElement_t*)REWRITE_ADDR(msg->addr,sharedMemoryUserBase,sharedMemoryKernelBase);
					// Rewrite all pointer within the datamodel
					rewriteDatamodelAddress(dm,sharedMemoryUserBase,sharedMemoryKernelBase);
					ACQUIRE_WRITE_LOCK(slcLock);
					// It is not necessary to copy the datamodel, because mergeDataModel will do this in order to merge it into the existing model
					ret = mergeDataModel(0,SLC_DATA_MODEL,dm);
					if (ret < 0) {
						ERR_MSG("Weird! Cannot merge datamodel received by userspace!\n");
					}
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_DM_DEL:
					dm = (DataModelElement_t*)REWRITE_ADDR(msg->addr,sharedMemoryUserBase,sharedMemoryKernelBase);
					// Rewrite all pointer within the datamodel
					rewriteDatamodelAddress(dm,sharedMemoryUserBase,sharedMemoryKernelBase);
					ACQUIRE_WRITE_LOCK(slcLock);
					ret = deleteSubtree(&SLC_DATA_MODEL,dm);
					if (ret < 0) {
						ERR_MSG("Weird! Cannot delete datamodel received by userspace!\n");
					}
					if (SLC_DATA_MODEL == NULL) {
						initSLCDatamodel();
					}
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_QUERY_ADD:
					query = (Query_t*)REWRITE_ADDR(msg->addr,sharedMemoryUserBase,sharedMemoryKernelBase);
					//rewriteQueryAddress(query,sharedMemoryUserBase,sharedMemoryKernelBase);
					// Allocate memory, because it is necessary to copy the query to this layer. Currently query points to the shared memory.
					queryCopy = ALLOC(query->size);
					if (queryCopy == NULL) {
						break;
					}
					memcpy(queryCopy,query,query->size);
					// Rewrite all pointers within this query to be able to access it.
					rewriteQueryAddress(queryCopy,msg->addr,queryCopy);
					ACQUIRE_WRITE_LOCK(slcLock);
					if (addQueries(SLC_DATA_MODEL,queryCopy,&flags) != 0) {
						freeQuery(queryCopy);
					}
					DEBUG_MSG(2,"Registered remote query with id %d\n",queryCopy->queryID);
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_QUERY_DEL:
					queryID = (QueryID_t*)REWRITE_ADDR(msg->addr,sharedMemoryUserBase,sharedMemoryKernelBase);
					ACQUIRE_WRITE_LOCK(slcLock);
					// Try to resolve queryID to a pointer to a real query
					query = resolveQuery(SLC_DATA_MODEL,queryID);
					if (query == NULL) {
						ERR_MSG("No such query: name=%s, id=%d\n",queryID->name, queryID->id);
						RELEASE_READ_LOCK(slcLock);
						break;
					}
					delQueries(SLC_DATA_MODEL,query,&flags);
					/*
					 * delQueries() does *not* free the query itself.
					 * Normally a query is handed over by a module/shared library and directly registered.
					 * Therefore, the module/shared library has to free. In this case the query was handed over by the remote layer.
					 * So, it is freed now.
					 */
					freeQuery(query);
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_QUERY_CONTINUE:
					queryCont = (QueryContinue_t*)REWRITE_ADDR(msg->addr,sharedMemoryUserBase,sharedMemoryKernelBase);
					ACQUIRE_READ_LOCK(slcLock);
					// Try to resolve queryID to a pointer to a real query
					query = resolveQuery(SLC_DATA_MODEL,&queryCont->qID);
					if (query == NULL) {
						ERR_MSG("No such query: name=%s, id=%d\n",queryCont->qID.name, queryCont->qID.id);
						RELEASE_READ_LOCK(slcLock);
						break;
					}
					curTupleShm = (Tupel_t*)(queryCont + 1);
					headTupleCopy = NULL;
					prevTupleCopy = NULL;
					ret = 0;
					// Basically a MSG_QUERY_CONTINUE can have one or more tuples attached
					do {
						rewriteTupleAddress(SLC_DATA_MODEL,curTupleShm,sharedMemoryUserBase,sharedMemoryKernelBase);
						curTupleCopy = copyTupel(SLC_DATA_MODEL,curTupleShm);
						if (curTupleCopy == NULL) {
							ERR_MSG("Cannot copy tuple from shared memory. Freeing all previous copied tuples. Query: name=%s, id=%d\n", queryCont->qID.name, queryCont->qID.id);
							curTupleCopy = headTupleCopy;
							// There was an error during copying curTupleShm --> free all tuples copied so far
							while (curTupleCopy != NULL) {
								prevTupleCopy = curTupleCopy->next;
								freeTupel(SLC_DATA_MODEL,curTupleCopy);
								curTupleCopy = prevTupleCopy;
							}
							headTupleCopy = NULL;
							break;
						}
						if (prevTupleCopy != NULL) {
							prevTupleCopy->next = curTupleCopy;
						}
						if (headTupleCopy == NULL) {
							headTupleCopy = curTupleCopy;
						}
						prevTupleCopy = curTupleCopy;
						curTupleShm = curTupleShm->next;
						ret++;
					} while (curTupleShm != NULL);
					if (headTupleCopy != NULL) {
						// Handover the tuples and the query to the executor
						DEBUG_MSG(2,"Enqueueing %d remote tuple(s) for execution.\n",ret);
						enqueueQuery(query,headTupleCopy,queryCont->steps);
					}
					RELEASE_READ_LOCK(slcLock);
					break;

				case MSG_DM_SNAPSHOT:
					// the userspace part requests a complete snapshot of our datamodel.
					DEBUG_MSG(2,"Userspace requested a complete snapshot of our datamode. Give to him... (aha aha)\n");
					ACQUIRE_WRITE_LOCK(slcLock);
					sendDatamodel(SLC_DATA_MODEL,MSG_DM_SNAPSHOT);
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_EMPTY:
					ERR_MSG("Read empty message!\n");
					break;

				default:
					ERR_MSG("Unknown message type: 0x%x\n",msg->type);
			}
			ringBufferReadEnd(rxBuffer);
		}
		sleepTime = calcSleepTime(sleepTime,msg != NULL);
		DEBUG_MSG(3,"sleepTime = %d us\n",sleepTime);
	}

	return 0;
}

static void communicationFileMmapOpen(struct vm_area_struct *vma) {
	DataModelMmap_t *info = (DataModelMmap_t *)vma->vm_private_data;

	info->reference++;
	atomic_inc(&communicationFileMmapRef);
	DEBUG_MSG(3,"reference (open) = %d\n",info->reference);
}

static void communicationFileMmapClose(struct vm_area_struct *vma) {
	DataModelMmap_t *info = (DataModelMmap_t *)vma->vm_private_data;

	info->reference--;
	atomic_dec(&communicationFileMmapRef);
	DEBUG_MSG(3,"reference (close) = %d\n",info->reference);
}

/* nopage is called the first time a memory area is accessed which is not in memory,
 * it does the actual mapping between kernel and user space memory
 */
//struct page *mmap_nopage(struct vm_area_struct *vma, unsigned long address, int *type)	--changed
static int communicationFileMmapFault(struct vm_area_struct *vma, struct vm_fault *vmf) {
	struct page *page;
	DataModelMmap_t *info;

	/* the data is in vma->vm_private_data */
	info = (DataModelMmap_t *)vma->vm_private_data;
	if (!info->data) {
		ERR_MSG("No VMA private data!\n");
		return VM_FAULT_SIGBUS;
	}

	if (vmf->pgoff > NUM_PAGES) {
		ERR_MSG("Page offset is to large: %ld > %d\n",vmf->pgoff,NUM_PAGES);
		return VM_FAULT_SIGBUS;
	}
	page = sharedMemoryPages[vmf->pgoff];
	DEBUG_MSG(2,"page fault at 0x%p, mapping page 0x%p at offset %ld\n",vmf->virtual_address,page_address(page),vmf->pgoff);
	/* increment the reference count of this page */
	get_page(page);
	// Tell the memory management which page should be mapped at (vmf->virtual_address & PAGE_MASK)
	vmf->page = page;

	return 0;
}

struct vm_operations_struct communicationFile_mmap_ops = {
	.open		=	communicationFileMmapOpen,
	.close		=	communicationFileMmapClose,
	.fault		=	communicationFileMmapFault,
};

static ssize_t communicationFileRead(struct file *fil, char __user *buffer, size_t buffer_length, loff_t *pos) {
	int ret = 0;

	if (*pos > 0) {
		return 0;
	}
	ret = snprintf(buffer,buffer_length, "0x%lx\n",(unsigned long)sharedMemoryKernelBase);
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

static int communicationFileMmap(struct file *filp, struct vm_area_struct *vma) {
	DataModelMmap_t *info = NULL;

	if (atomic_read(&communicationFileMmapRef) >= 1) {
		return -EBUSY;
	}

	vma->vm_ops = &communicationFile_mmap_ops;
	vma->vm_flags |= VM_RESERVED;
	/* assign the file private data to the vm private data */
	vma->vm_private_data = filp->private_data;

	info = (DataModelMmap_t*)vma->vm_private_data;
	DEBUG_MSG(2,"mapping datamodel space at 0x%lx from 0x%lx to 0x%lx\n",info->data,vma->vm_start,vma->vm_end);
	sharedMemoryUserBase = (void*)vma->vm_start;

	communicationFileMmapOpen(vma);
	return 0;
}

static int communicationFileClose(struct inode *inode, struct file *filp) {
	DataModelMmap_t *info = (DataModelMmap_t*)filp->private_data;

	kfree(info);
	filp->private_data = NULL;

	return 0;
}

static int communicationFileOpen(struct inode *inode, struct file *filp) {
	DataModelMmap_t *info = NULL;

	info = kmalloc(sizeof(DataModelMmap_t), GFP_KERNEL);
	if (info == NULL) {
		return -ENOMEM;
	}
	/* obtain new memory */
	info->data = (unsigned long)sharedMemoryKernelBase;
	/* assign this info struct to the file */
	filp->private_data = info;
	info->reference = 0;

	return 0;
}

static const struct file_operations proc_datamodel_operations = {
	.open		=	communicationFileOpen,
	.read		=	communicationFileRead,
	.release	=	communicationFileClose,
	.mmap		=	communicationFileMmap,
};

static int __init slc_init(void) {
	int i = 0, j = 0;
	kuid_t fileUID;
	kgid_t fileGID;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO - 1 };

	fileUID.val = 0;
	fileGID.val = 0;
	// Allocate the pointer array for our pages used by the shared memory
	sharedMemoryPages = kmalloc(sizeof(struct page*) * NUM_PAGES,GFP_KERNEL);
	if (sharedMemoryPages == NULL) {
		ERR_MSG("Cannot allocate memory for sharedMemoryPages\n");
		return -ENOMEM;
	}
	// Allocate NUM_PAGE physical, single pages. They are *not* consecutive.
	for (i = 0; i < NUM_PAGES; i++) {
		sharedMemoryPages[i] = alloc_page(GFP_KERNEL|__GFP_ZERO);
		if (sharedMemoryPages[i] == NULL) {
			ERR_MSG("Cannot allocate page\n");
			for (j = 0; j < i; j++) {
				__free_page(sharedMemoryPages[j]);
			}
			kfree(sharedMemoryPages);
			return -ENOMEM;
		}
	}
	// Map the NUM_PAGE pages to one virtual memory area.
	sharedMemoryKernelBase = vmap(sharedMemoryPages,NUM_PAGES,VM_MAP,PAGE_KERNEL);
	if (sharedMemoryKernelBase == NULL) {
		ERR_MSG("Cannot vamp allocated pages\n");
		for (j = 0; j < NUM_PAGES; j++) {
			__free_page(sharedMemoryPages[j]);
		}
		kfree(sharedMemoryPages);
	}
	INFO_MSG("Allocated %d pages and mapped them to address 0x%p\n",NUM_PAGES,sharedMemoryKernelBase);
	ringBufferInit();

	procfsSlcDir = proc_mkdir(PROCFS_DIR_NAME, NULL);
	if (procfsSlcDir == NULL) {
		printk("Error: Could not initialize /proc/%s\n",PROCFS_DIR_NAME);
		return -ENOMEM;
	}
	proc_set_user(procfsSlcDir,fileUID,fileGID);

	procfsSlcDMFile = proc_create(PROCFS_COMMFILE, 0777, procfsSlcDir,&proc_datamodel_operations);
	if (procfsSlcDMFile == NULL) {
		proc_remove(procfsSlcDir);
		printk("Error: Could not initialize /proc/%s/%s\n",PROCFS_DIR_NAME,PROCFS_COMMFILE);
		return -ENOMEM;
	}
	proc_set_user(procfsSlcDMFile,fileUID,fileGID);
	proc_set_size(procfsSlcDMFile,7);

	atomic_set(&communicationFileMmapRef,0);

	if (initSLC() == -1) {
		return -1;
	}

	atomic_set(&waitingQueries,0);
	atomic_set(&missedTimer,0);
	maxWaitingQueries = 0;
#ifdef CALC_SLEEP_TIME
	successfullReads = 0;
	failedReads = 0;
#endif
	sleepTime = INIT_SLEEP_TIME;
	// Init ...
	queryExecThread = (struct task_struct*)kthread_create(queryExecutorWork,NULL,"queryExecThread");
	if (IS_ERR(queryExecThread)) {
		return PTR_ERR(queryExecThread);
	}
	// ... and start the query execution thread
	wake_up_process(queryExecThread);
	if (useRTPrio) {
		if (sched_setscheduler(queryExecThread, SCHED_FIFO, &param) == 0) {
			INFO_MSG("Assigend real-time priority to queryExecThread\n");
		} else {
			ERR_MSG("Cannot assign real-time priority to queryExecThread\n");
		}
	} else {
		INFO_MSG("Running queryExecThread at normal priority\n");
	}

	// Init ...
	commThread = (struct task_struct*)kthread_create(commThreadWork,NULL,"commThread");
	if (IS_ERR(commThread)) {
		kthread_stop(queryExecThread);
		return PTR_ERR(commThread);
	}
	// ... and start the communication thread which reads from the rxBuffer and processes the received messages.
	wake_up_process(commThread);

	INFO_MSG("Initialized SLC\n");
	return 0;
}

static void __exit slc_exit(void) {
	int i = 0;
	
	// Signal the query execution thread to terminate and wait for it
	kthread_stop(queryExecThread);
	queryExecThread = NULL;
	// Signal the communication execution thread to terminate and wait for it
	kthread_stop(commThread);
	commThread = NULL;

	proc_remove(procfsSlcDMFile);
	proc_remove(procfsSlcDir);

	destroySLC();

	vunmap(sharedMemoryKernelBase);
	for (i = 0; i < NUM_PAGES; i++) {
		__free_page(sharedMemoryPages[i]);
	}
	kfree(sharedMemoryPages);

	INFO_MSG("Max amount of outstanding queries: %lu\n",maxWaitingQueries);
	INFO_MSG("Missed %d timer\n", atomic_read(&missedTimer));
	INFO_MSG("Skipped the sending of %d query continue message\n",skippedQueryCont);
	INFO_MSG("Destroyed SLC\n");
}

module_init(slc_init);
module_exit(slc_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
