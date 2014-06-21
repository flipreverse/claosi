#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <datamodel.h>
#include <resultset.h>
#include <query.h>
#include <api.h>

static struct task_struct *queryExecThread = NULL;
static LIST_HEAD(queriesToExecList);
// Synchronize access to queriesToExecList
static DEFINE_MUTEX(listLock);
static DECLARE_WAIT_QUEUE_HEAD(waitQueue);
/*
 * Using list_empty() as a condition for wait_event() may lead to a race condition, especially on a smp system.
 * One processor executes enqueueQuery() while another one checks the condition '!list_empty(queriesToExecList)'.
 * So, we have to use a datatype which allows us atomically acess *and* represents the lists status.
 * The waitingQueries variable holds the number of outstanding queries.
 */ 
static atomic_t waitingQueries;

typedef struct QueryJob {
	struct list_head list;
	Query_t *query;
	Tupel_t *tuple;
} QueryJob_t;

void enqueueQuery(Query_t *query, Tupel_t *tuple) {
	QueryJob_t *job = NULL;

	/*
	 * Honestly, it is not necessary to check, if the execution thread is running.
	 * The module cannot be unloaded while there is at least one registered provider remaining.
	 * Therefore, it's impossible that neither the lock (listLock) nor the list (queriesToExecList)
	 * are NULL.
	 */
	mutex_lock(&listLock);
	job = ALLOC(sizeof(QueryJob_t));
	if (job == NULL) {
		mutex_unlock(&listLock);
		DEBUG_MSG(1,"Cannot allocate memory for QueryJob_t\n");
		return;
	}
	job->query = query;
	job->tuple = tuple;
	// Enqueue it
	list_add_tail(&job->list,&queriesToExecList);
	atomic_inc(&waitingQueries);
	mutex_unlock(&listLock);

	DEBUG_MSG(2,"Enqueued query 0x%x with tuple %p for execution\n",job->query->queryID,job->tuple);
	// Notify the query execution about the outstanding query
	wake_up(&waitQueue);
}
EXPORT_SYMBOL(enqueueQuery);

static int queryExecutorWork(void *data) {
	QueryJob_t *cur = NULL;
	unsigned long flags;
	DEBUG_MSG(2,"Started execution thread\n");

	while (1) {
		DEBUG_MSG(3,"%s: Waiting for incoming queries...\n",__FUNCTION__);
		
		wait_event(waitQueue,kthread_should_stop() || atomic_read(&waitingQueries) > 0);

		while (atomic_read(&waitingQueries) > 0) {
			mutex_lock(&listLock);
			// Dequeue the head and execute the query
			cur = list_first_entry(&queriesToExecList,QueryJob_t,list);
			list_del(&cur->list);
			atomic_dec(&waitingQueries);
			mutex_unlock(&listLock);

			DEBUG_MSG(3,"%s: Executing query 0x%x with tuple %p\n",__FUNCTION__,cur->query->queryID,cur->tuple);
			ACQUIRE_READ_LOCK(slcLock);
			executeQuery(slcDataModel,cur->query,&cur->tuple);
			RELEASE_READ_LOCK(slcLock);
			FREE(cur);
		}
		if (kthread_should_stop()) {
			DEBUG_MSG(3,"%s: Were asked to terminate.\n",__FUNCTION__);
			break;
		}
	}
	do_exit(0);
	
	return 0;
}

static int __init slc_init(void) {
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
	FREE(queryExecThread);

	DEBUG_MSG(1,"Destroyed SLC\n");
}

module_init(slc_init);
module_exit(slc_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
