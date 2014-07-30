#include <api.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#define SEM_KEY 0xcaffee
#define TIMER_SIGNAL SIGRTMIN

static int fdDataModelFile  = 0;
/**
 * A descriptor of the executor thread
 */
static pthread_t queryExecThread;
/**
 * pthread attributes: needed to mark a thread as joinable
 */
static pthread_attr_t queryExecThreadAttr;
static int queryExecThreadRunning = 0;
/**
 * ID of the semaphore to synchronize enqueueQuery() and the execution thread
 * enqueueQuery() will execute a v() and the thread a p().
 */
static int waitingQueriesSemID = 0;
/**
 * A simple mutex to synchronize the access to the list holding all remaining queries
 */
static pthread_mutex_t listLock;
/**
 * A list head for the list of remaining queries
 */
STAILQ_HEAD(QueryExecListHead,QueryJob) queriesToExecList;

 union semun {
	int val;					/* Value for SETVAL */
	struct semid_ds *buf;		/* Buffer for IPC_STAT, IPC_SET */
	unsigned short  *array;		/* Array for GETALL, SETALL */
	struct seminfo  *__buf;		/* Buffer for IPC_INFO (Linux-specific) */
};

//static void timerHandler(int sig, siginfo_t *si, void *uc) {
static void timerHandler(union sigval data) {
	QueryTimerJob_t *timerJob = (QueryTimerJob_t*)data.sival_ptr;
	Source_t *src = (Source_t*)timerJob->dm->typeInfo;
	Tupel_t *tuple= NULL;

	DEBUG_MSG(3,"Context of timerHandler, tid=%ld\n",syscall(SYS_gettid));
	DEBUG_MSG(3,"%s: Creating tuple\n",__FUNCTION__);
	// Only one timer at a time is allowed to access this source
	ACQUIRE_WRITE_LOCK(src->lock);
	tuple = src->callback();
	RELEASE_WRITE_LOCK(src->lock);
	if (tuple != NULL) {
		DEBUG_MSG(3,"%s: Enqueue tuple\n",__FUNCTION__);
		enqueueQuery(timerJob->query,tuple);
	}
}

static void* queryExecutorWork(void *data) {
	int ret = 0;
	struct sembuf operation;
	QueryJob_t *cur = NULL;
	
	operation.sem_num = 0;
	operation.sem_flg = 0;
	operation.sem_op = -1;
	DEBUG_MSG(3,"Started execution thread\n");

	while (1) {
		DEBUG_MSG(3,"%s: Waiting for incoming queries...\n",__FUNCTION__);
		ret = semop(waitingQueriesSemID,&operation,1);
		if (ret < 0) {
			if (queryExecThreadRunning == 0) {
				DEBUG_MSG(3,"%s: Were asked to terminate.\n",__FUNCTION__);
				break;
			} else if (errno == EINTR) {
				// Nothing to do
				continue;
			} else {
				ERR_MSG("Semop failed: %s\n",strerror(errno));
				continue;
			}
		}

		pthread_mutex_lock(&listLock);
		// Dequeue the head and execute the query
		cur = STAILQ_FIRST(&queriesToExecList);
		STAILQ_REMOVE_HEAD(&queriesToExecList,listEntry);
		pthread_mutex_unlock(&listLock);

		DEBUG_MSG(3,"%s: Executing query 0x%x with tuple %p\n",__FUNCTION__,cur->query->queryID,cur->tuple);
		// A queries execution just reads from the datamodel. No write lock is needed.
		ACQUIRE_READ_LOCK(slcLock);
		executeQuery(SLC_DATA_MODEL,cur->query,&cur->tuple);
		RELEASE_READ_LOCK(slcLock);
		FREE(cur);
	}

	pthread_exit(0);
	return NULL;
}

void enqueueQuery(Query_t *query, Tupel_t *tuple) {
	QueryJob_t *job = NULL;
	struct sembuf operation;
	operation.sem_num = 0;
	operation.sem_flg = 0;
	operation.sem_op = 1;

	/*
	 * Honestly, it is not necessary to check, if the execution thread is running.
	 * The module cannot be unloaded while there is at least one registered provider remaining.
	 * Therefore, it's impossible that neither the lock (listLock) nor the list (queriesToExecList)
	 * are NULL.
	 */
	job = ALLOC(sizeof(QueryJob_t));
	if (job == NULL) {
		ERR_MSG("Cannot allocate memory for QueryJob_t\n");
		return;
	}
	job->query = query;
	job->tuple = tuple;
	// Enqueue it
	DEBUG_MSG(3,"Enqueued query 0x%x with tuple %p for execution\n",job->query->queryID,job->tuple);
	pthread_mutex_lock(&listLock);
	STAILQ_INSERT_TAIL(&queriesToExecList,job,listEntry);
	pthread_mutex_unlock(&listLock);
	// Signal the execution thread a query is ready for execution
	if (semop(waitingQueriesSemID,&operation,1) < 0) {
		ERR_MSG("Error executing p() von semaphore: %s\n",strerror(errno));
	}
}

static void* generateObjectStatus(void *data) {
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
	pthread_exit(0);
	return NULL;
}

void startObjStatusThread(Query_t *query, generateStatus statusFn) {
	pthread_t objStatusThread;
	QueryStatusJob_t *statusJob = NULL;

	// Allocate memory for the threads parameters
	statusJob = ALLOC(sizeof(QueryStatusJob_t));
	if (statusJob == NULL) {
		ERR_MSG("Cannot allocate memory for QueryStatusJob_t\n");
		return;
	}
	// Pass the query as well as the function pointer to the query
	statusJob->query = query;
	statusJob->statusFn = statusFn;
	// In contrast to the kernel part it is *not* necessary to release and re-acquire the slc lock. Just start the pthread.
	if (pthread_create(&objStatusThread,NULL,generateObjectStatus,statusJob) < 0) {
		ERR_MSG("Cannot allocate cerate objStatusThread: %s\n",strerror(errno));
		FREE(statusJob);
		return;
	}
}

void startSourceTimer(DataModelElement_t *dm, Query_t *query) {
	SourceStream_t *srcStream = (SourceStream_t*)query->root;
	QueryTimerJob_t *timerJob = NULL;
	// Allocate memory for the job-specific information; job-specific = (query,datamodel,period)
	timerJob = ALLOC(sizeof(QueryTimerJob_t));
	if (timerJob == NULL) {
		ERR_MSG("Cannot allocate memory for QueryTimerJob_t\n");
		return;
	}

	DEBUG_MSG(1,"%s: Init posix timer for node %s\n",__FUNCTION__,srcStream->st_name);
	timerJob->period = srcStream->period;
	timerJob->query = query;
	timerJob->dm = dm;
	/*
	 * the timer will generate the TIMER_SIGNAL signal upon expiration.
	 * Each pending signal will be processed in a dedicated thread.
	 * This will avoid race conditions between subsequent calls to timerHandler.
	 * The letter would be the case, if sigev_notify is set to SIGEV_SIGNAL.
	 */
	timerJob->timerNotify.sigev_notify = SIGEV_THREAD;
	timerJob->timerNotify.sigev_signo = TIMER_SIGNAL;
	// Pass a pointer to QueryTimerJob_t to enable the handler to access the query and the source function pointer.
	timerJob->timerNotify.sigev_value.sival_ptr = timerJob;
	timerJob->timerNotify.sigev_notify_function = timerHandler;
	timerJob->timerValue.it_value.tv_sec = timerJob->period / 1000;
	timerJob->timerValue.it_value.tv_nsec = (timerJob->period % 1000) * 1000000;
	timerJob->timerValue.it_interval.tv_sec = timerJob->timerValue.it_value.tv_sec;
	timerJob->timerValue.it_interval.tv_nsec = timerJob->timerValue.it_value.tv_nsec;
	// Setup the timer using timing information relative to the current clock which will be the monotonic one.
	if (timer_create(CLOCK_REALTIME,&timerJob->timerNotify,&timerJob->timerID) < 0) {
		ERR_MSG("timer_create failed: %s\n",strerror(errno));
		FREE(timerJob);
		return;
	}
	DEBUG_MSG(1,"%s: Starting posixtimer for node %s. Will fire in %u ms.\n",__FUNCTION__,srcStream->st_name,srcStream->period);
	// Fire it up.... :-)
	if (timer_settime(timerJob->timerID, 0, &timerJob->timerValue, NULL) < 0) {
		ERR_MSG("timer_settime failed: %s\n",strerror(errno));
		FREE(timerJob);
		return;
	}
	srcStream->timerInfo = timerJob;
}

void stopSourceTimer(Query_t *query) {
	int ret = 0;
	SourceStream_t *srcStream = (SourceStream_t*)query->root;
	QueryTimerJob_t *timerJob = (QueryTimerJob_t*)srcStream->timerInfo;
	struct itimerspec timerValue; 

	timerValue.it_value.tv_sec = 0;
	timerValue.it_value.tv_nsec = 0;
	timerValue.it_interval.tv_sec = 0;
	timerValue.it_interval.tv_nsec = 0;
	// Setting the next expiration to 0 will cancel the timer
	if (timer_settime(timerJob->timerID,0,&timerValue,NULL) < 0) {
		ERR_MSG("timer_settime - delete - failed: %s\n",strerror(errno));
		return;
	}
	DEBUG_MSG(1,"%s: Canceling posix timer for node %s...\n",__FUNCTION__,srcStream->st_name);
	// Delete the timer
	ret = timer_delete(timerJob->timerID);
	if (ret < 0) {
		ERR_MSG("timer_delete failed: %s\n",strerror(errno));
	}
	DEBUG_MSG(1,"%s: posix timer for node %s canceled. Was active: %d\n",__FUNCTION__,srcStream->st_name,ret);
	// Free the timer information
	FREE(timerJob);
	srcStream->timerInfo = NULL;
}

int initLayer(void) {
	union semun cmdval;

	fdDataModelFile = open("/proc/" PROCFS_DIR_NAME "/" PROCFS_DATAMODELFILE, O_RDWR);
	if (fdDataModelFile < 0) {
		ERR_MSG("Cannot open datamodel file: %s\n",strerror(errno));
		return -1;
	}

	DEBUG_MSG(1,"Trying to map the kernels shared memory\n");
	sharedMemoryBaseAddr = mmap(NULL, PAGE_SIZE * NUM_PAGES, PROT_READ|PROT_WRITE, MAP_PRIVATE, fdDataModelFile, 0);
	if (sharedMemoryBaseAddr == MAP_FAILED) {
		ERR_MSG("Cannot mmap datamodel file: %s\n",strerror(errno));
		close(fdDataModelFile);
		return -1;
	}
	DEBUG_MSG(1,"Mapped the kernels shared memory at %p\n",sharedMemoryBaseAddr);

	// Create the semaphore for the query list and ...
	waitingQueriesSemID = semget(SEM_KEY,1,IPC_CREAT|IPC_EXCL|0600);
	if (waitingQueriesSemID < 0) {
		ERR_MSG("semget failed: %s\n",strerror(errno));
		return -1;
	}
	cmdval.val = 0;
	// ... initialize it to 0.
	if (semctl(waitingQueriesSemID,0,SETVAL,cmdval) < 0) {
		ERR_MSG("semctl failed: %s\n",strerror(errno));
		return -1;
	}
	// Init list lock and the list itself
	pthread_mutex_init(&listLock,NULL);
	STAILQ_INIT(&queriesToExecList);
	// Set up the query execution thread as joinable and start it.
	queryExecThreadRunning = 1;
	pthread_attr_init(&queryExecThreadAttr);
	pthread_attr_setdetachstate(&queryExecThreadAttr, PTHREAD_CREATE_JOINABLE);
	if (pthread_create(&queryExecThread,&queryExecThreadAttr,queryExecutorWork,NULL) < 0) {
		ERR_MSG("Cannot create queryExecThread: %s\n",strerror(errno));
		return -1;
	}
	return 0;
}

void exitLayer(void) {
	queryExecThreadRunning = 0;
	// Destroy the semaphore and wait for the execution thread to terminate
	semctl(waitingQueriesSemID,0,IPC_RMID);
	pthread_join(queryExecThread,NULL);
	munmap(sharedMemoryBaseAddr, NUM_PAGES * PAGE_SIZE);
	close(fdDataModelFile);
}
