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
#include <communication.h>
#include <api.h>

#define SEM_KEY 0xcaffee
#define TIMER_SIGNAL SIGRTMIN

static int fdCommunicationFile  = 0;
/**
 * A descriptor of the communication thread
 */
static pthread_t commThread;
/**
 * pthread attributes: needed to mark a thread as joinable
 */
static pthread_attr_t commThreadAttr;
static int commThreadRunning = 0;
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
static int missedTimer;

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

	/**
	 * It is possible that the component deletes queries thus stopping a timer while a timer expires.
	 * Hence, the hrtimerHandler while stall at src->callback(), because the source wants to acquire the slcLock in order to create a tuple.
	 * After deleting the queries the lock will be release and the handler will acquire it.
	 * It is likeley that this timer was canceled. Therefore, the instance of TimerJob_t was freed. Further access to the recently freed memory location
	 * will lead to unpredictable behavior.
	 * To avoid this, each call to hrtimerHandler() will try to acquire the read lock. If it fails, the handler will abort.
	 * The delQuery() function will acquire a write lock.
	 */
	if (TRY_READ_LOCK(slcLock) != 0) {
		ERR_MSG("Cannot acquire read timer lock\n");
		__sync_fetch_and_add(&missedTimer,1);
		return;
	}
	DEBUG_MSG(3,"Context of timerHandler, tid=%ld\n",syscall(SYS_gettid));
	DEBUG_MSG(3,"%s: Creating tuple\n",__FUNCTION__);
	// Only one timer at a time is allowed to access this source
	ACQUIRE_WRITE_LOCK(src->lock);
	tuple = src->callback();
	RELEASE_WRITE_LOCK(src->lock);
	
	if (tuple != NULL) {
		DEBUG_MSG(3,"%s: Enqueue tuple\n",__FUNCTION__);
		enqueueQuery(timerJob->query,tuple,0);
	}
	RELEASE_READ_LOCK(slcLock);
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

		ACQUIRE_READ_LOCK(slcLock);
		pthread_mutex_lock(&listLock);
		// Dequeue the head and execute the query
		cur = STAILQ_FIRST(&queriesToExecList);
		STAILQ_REMOVE_HEAD(&queriesToExecList,listEntry);
		pthread_mutex_unlock(&listLock);

		DEBUG_MSG(3,"%s: Executing query 0x%x with tuple %p\n",__FUNCTION__,cur->query->queryID,cur->tuple);
		// A queries execution just reads from the datamodel. No write lock is needed.
		executeQuery(SLC_DATA_MODEL,cur->query,cur->tuple,cur->step);
		RELEASE_READ_LOCK(slcLock);
		FREE(cur);
	}

	pthread_exit(0);
	return NULL;
}

static void* commThreadWork(void *data) {
	LayerMessage_t *msg = NULL;
	DataModelElement_t *dm = NULL;
	Query_t *query = NULL, *queryCopy = NULL;
	QueryID_t *queryID = NULL;
	int ret = 0;

	while (commThreadRunning == 1) {
		msg = ringBufferReadBegin(rxBuffer);
		if (msg == NULL) {
			sleep(1);
		} else {
			DEBUG_MSG(1,"Read msg with type 0x%x and addr %p (rewritten addr = %p)\n",msg->type,msg->addr,REWRITE_ADDR(msg->addr,sharedMemoryUserBase,sharedMemoryKernelBase));
			switch (msg->type) {
				case MSG_DM_ADD:
					dm = (DataModelElement_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
					rewriteDatamodelAddress(dm,sharedMemoryKernelBase,sharedMemoryUserBase);
					ACQUIRE_WRITE_LOCK(slcLock);
					ret = mergeDataModel(0,SLC_DATA_MODEL,dm);
					if (ret < 0) {
						ERR_MSG("Weird! Cannot merge datamodel received by kernel!\n");
					}
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_DM_DEL:
					dm = (DataModelElement_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
					rewriteDatamodelAddress(dm,sharedMemoryKernelBase,sharedMemoryUserBase);
					ACQUIRE_WRITE_LOCK(slcLock);
					ret = deleteSubtree(&SLC_DATA_MODEL,dm);
					if (ret < 0) {
						ERR_MSG("Weird! Cannot delete datamodel received by kernel!\n");
					}
					if (SLC_DATA_MODEL == NULL) {
						initSLCDatamodel();
					}
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_QUERY_ADD:
					DEBUG_MSG(1,"Received query\n");
					query = (Query_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
					rewriteQueryAddress(query,sharedMemoryKernelBase,sharedMemoryUserBase);
					DEBUG_MSG(1,"Rewrote query\n");
					queryCopy = ALLOC(query->size);
					if (queryCopy == NULL) {
						break;
					}
					memcpy(queryCopy,query,query->size);
					rewriteQueryAddress(queryCopy,query,queryCopy);
					DEBUG_MSG(1,"Rewrote copied query, compact? %d\n",queryCopy->flags & COMPACT);
					ACQUIRE_WRITE_LOCK(slcLock);
					if (addQueries(SLC_DATA_MODEL,queryCopy) != 0) {
						freeQuery(queryCopy);
					}
					DEBUG_MSG(1,"Registered remote query with id %d\n",queryCopy->queryID);
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_QUERY_DEL:
					DEBUG_MSG(1,"Received del query\n");
					queryID = (QueryID_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
					ACQUIRE_READ_LOCK(slcLock);
					query = resolveQuery(queryID);
					if (query == NULL) {
						ERR_MSG("No such query: name=%s, id=%d\n",queryID->name, queryID->id);
						RELEASE_READ_LOCK(slcLock);
						break;
					}
					delQueries(SLC_DATA_MODEL,query);
					freeQuery(query);
					RELEASE_READ_LOCK(slcLock);
					break;

				case MSG_QUERY_CONTINUE:
					break;


				case MSG_EMPTY:
					ERR_MSG("Read empty message!\n");
					break;

				default:
					ERR_MSG("Unknown message type: 0x%x\n",msg->type);
			}
			ringBufferReadEnd(rxBuffer);
		}
	}

	pthread_exit(0);
	return NULL;
}

void enqueueQuery(Query_t *query, Tupel_t *tuple, int step) {
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
	job->step = step;
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

void delPendingQuery(Query_t *query) {
	QueryJob_t *cur = NULL, *curTmp = NULL;
	
	pthread_mutex_lock(&listLock);
	for (cur = STAILQ_FIRST(&queriesToExecList); cur != NULL; cur = curTmp) {
		curTmp = STAILQ_NEXT(cur,listEntry);
		if (cur->query == query) {
			DEBUG_MSG(1,"Found query 0x%lx. Removing it from list.\n",(unsigned long)cur->query);
			STAILQ_REMOVE(&queriesToExecList,cur,QueryJob,listEntry);
		}
	}
	pthread_mutex_unlock(&listLock);
}

static void* generateObjectStatus(void *data) {
	QueryStatusJob_t *statusJob = (QueryStatusJob_t*)data;
	Tupel_t *curTuple = NULL;

	// The object may return a linked-list of Tuple_t
	curTuple = statusJob->statusFn();
	while (curTuple != NULL) {
		// Forward any query to the execution thread
		enqueueQuery(statusJob->query,curTuple,0);
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
	char buffer[20];
	unsigned long addr = 0;

	fdCommunicationFile = open("/proc/" PROCFS_DIR_NAME "/" PROCFS_COMMFILE, O_RDWR);
	if (fdCommunicationFile < 0) {
		ERR_MSG("Cannot open datamodel file: %s\n",strerror(errno));
		return -1;
	}

	DEBUG_MSG(2,"Trying to map the kernels shared memory\n");
	sharedMemoryUserBase = mmap(NULL, PAGE_SIZE * NUM_PAGES, PROT_READ|PROT_WRITE, MAP_SHARED, fdCommunicationFile, 0);
	if (sharedMemoryUserBase == MAP_FAILED) {
		ERR_MSG("Cannot mmap datamodel file: %s\n",strerror(errno));
		close(fdCommunicationFile);
		return -1;
	}
	DEBUG_MSG(1,"Mapped the kernels shared memory at %p\n",sharedMemoryUserBase);
	if (read(fdCommunicationFile,buffer,20) < 0) {
		ERR_MSG("Cannot read sharedMemoryKernelBase: %s\n",strerror(errno));
		close(fdCommunicationFile);
		return -1;
	}
	addr = strtoul(buffer,NULL,16);
	DEBUG_MSG(1,"Kernel mapped shared memory at 0x%lx\n",addr);
	sharedMemoryKernelBase = (void*)addr;
	ringBufferInit();

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
	// Set up the communication thread as joinable and start it.
	commThreadRunning = 1;
	pthread_attr_init(&commThreadAttr);
	pthread_attr_setdetachstate(&commThreadAttr, PTHREAD_CREATE_JOINABLE);
	if (pthread_create(&commThread,&commThreadAttr,commThreadWork,NULL) < 0) {
		ERR_MSG("Cannot create commThread: %s\n",strerror(errno));
		queryExecThreadRunning = 0;
		semctl(waitingQueriesSemID,0,IPC_RMID);
		pthread_join(queryExecThread,NULL);
		return -1;
	}

	return 0;
}

void exitLayer(void) {
	queryExecThreadRunning = 0;
	commThreadRunning = 0;
	// Destroy the semaphore and wait for the execution thread to terminate
	semctl(waitingQueriesSemID,0,IPC_RMID);
	pthread_join(queryExecThread,NULL);
	pthread_join(commThread,NULL);
	munmap(sharedMemoryUserBase, NUM_PAGES * PAGE_SIZE);
	close(fdCommunicationFile);
	
	pthread_mutex_destroy(&listLock);
	DEBUG_MSG(1,"Missed %d timer\n", missedTimer);
}