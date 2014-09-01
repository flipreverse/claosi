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
#define CALC_SLEEP_TIME
#undef CALC_SLEEP_TIME

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
#ifdef CALC_SLEEP_TIME
/**
 * Number of successfull reads from the rx buffer
 */
static int successfullReads = 0;
/**
 * Number of failed reads from the rx buffer
 */
static int failedReads = 0;
#endif
/**
 * Amount of time to sleep after a failed read from the rx buffer
 */
static int sleepTime = INIT_SLEEP_TIME;
/**
 * Account for the number of missed timers - have a look at timerHandler()
 */
static int missedTimer;
/**
 * Count the queued queries
 */
static int waitingQueries;
/**
 * Evaluate the maximum of waiting queries
 */
static int maxWaitingQueries;

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
	GenStream_t *stream = (GenStream_t*)timerJob->query->root;
	Tupel_t *curTuple= NULL, *tempTuple = NULL;

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
	curTuple = src->callback(stream->selectors,stream->selectorsLen);
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
		ret =__sync_fetch_and_sub(&waitingQueries,1);
		if (ret > maxWaitingQueries) {
			maxWaitingQueries = ret;
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
	ret = 1000;
	return ret;
}
#else
#define calcSleepTime(x,y)		(INIT_SLEEP_TIME)
#endif

static void* commThreadWork(void *data) {
	LayerMessage_t *msg = NULL;
	DataModelElement_t *dm = NULL;
	Query_t *query = NULL, *queryCopy = NULL;
	QueryContinue_t *queryCont = NULL;
	QueryID_t *queryID = NULL;
	Tupel_t *curTupleShm = NULL, *curTupleCopy = NULL, *headTupleCopy = NULL, *prevTupleCopy = NULL;
	int ret = 0;

	while (commThreadRunning == 1) {
		msg = ringBufferReadBegin(rxBuffer);
		if (msg == NULL) {
			usleep(sleepTime);
		} else {
			DEBUG_MSG(3,"Read msg with type 0x%x and addr %p (rewritten addr = %p)\n",msg->type,msg->addr,REWRITE_ADDR(msg->addr,sharedMemoryUserBase,sharedMemoryKernelBase));
			switch (msg->type) {
				case MSG_DM_SNAPSHOT:
					DEBUG_MSG(2,"Received a complete snapshot of our datamode.\n");
				case MSG_DM_ADD:
					dm = (DataModelElement_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
					// Rewrite all pointer within the datamodel
					rewriteDatamodelAddress(dm,sharedMemoryKernelBase,sharedMemoryUserBase);
					ACQUIRE_WRITE_LOCK(slcLock);
					// It is not necessary to copy the datamodel, because mergeDataModel will do this in order to merge it into the existing model
					ret = mergeDataModel(0,SLC_DATA_MODEL,dm);
					if (ret < 0) {
						ERR_MSG("Weird! Cannot merge datamodel received by kernel!\n");
					}
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_DM_DEL:
					dm = (DataModelElement_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
					// Rewrite all pointer within the datamodel
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
					query = (Query_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
					//rewriteQueryAddress(query,sharedMemoryKernelBase,sharedMemoryUserBase);
					// Allocate memory, because it is necessary to copy the query to this layer. Currently query points to the shared memory.
					queryCopy = ALLOC(query->size);
					if (queryCopy == NULL) {
						break;
					}
					memcpy(queryCopy,query,query->size);
					rewriteQueryAddress(queryCopy,msg->addr,queryCopy);
					ACQUIRE_WRITE_LOCK(slcLock);
					if (addQueries(SLC_DATA_MODEL,queryCopy) != 0) {
						freeQuery(queryCopy);
					}
					DEBUG_MSG(2,"Registered remote query with id %d\n",queryCopy->queryID);
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_QUERY_DEL:
					queryID = (QueryID_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
					ACQUIRE_WRITE_LOCK(slcLock);
					// Try to resolve queryID to a pointer to a real query
					query = resolveQuery(SLC_DATA_MODEL,queryID);
					if (query == NULL) {
						ERR_MSG("No such query: name=%s, id=%d\n",queryID->name, queryID->id);
						RELEASE_READ_LOCK(slcLock);
						break;
					}
					delQueries(SLC_DATA_MODEL,query);
					/*
					 * delQueries() does *not* free the query itself.
					 * Normally a query is handed over by a module/shared library and directly registered.
					 * Therefore, the module/shared library has to free it. In this case, the query was handed over by the remote layer.
					 * So, it is up to us to free it now.
					 */
					freeQuery(query);
					RELEASE_WRITE_LOCK(slcLock);
					break;

				case MSG_QUERY_CONTINUE:
					queryCont = (QueryContinue_t*)REWRITE_ADDR(msg->addr,sharedMemoryKernelBase,sharedMemoryUserBase);
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
					// Basically, a MSG_QUERY_CONTINUE can have one or more tuples attached
					do {
						rewriteTupleAddress(SLC_DATA_MODEL,curTupleShm,sharedMemoryKernelBase,sharedMemoryUserBase);
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
#ifdef EVALUATION
	job->tuple->timestamp2 = getCycles();
#endif
	// Enqueue it
	DEBUG_MSG(3,"Enqueued query 0x%x with tuple %p for execution\n",job->query->queryID,job->tuple);
	pthread_mutex_lock(&listLock);
	STAILQ_INSERT_TAIL(&queriesToExecList,job,listEntry);
	pthread_mutex_unlock(&listLock);
	__sync_add_and_fetch(&waitingQueries,1);
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
			freeTupel(SLC_DATA_MODEL,cur->tuple);
			STAILQ_REMOVE(&queriesToExecList,cur,QueryJob,listEntry);
			FREE(cur);
		}
	}
	pthread_mutex_unlock(&listLock);
}

static void* generateObjectStatus(void *data) {
	QueryStatusJob_t *statusJob = (QueryStatusJob_t*)data;
	Tupel_t *curTuple = NULL, *tempTuple = NULL;
	GenStream_t *stream = (GenStream_t*)statusJob->query->root;

	// The object may return a linked-list of Tuple_t
	curTuple = statusJob->statusFn(stream->selectors,stream->selectorsLen);
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

	DEBUG_MSG(2,"%s: Init posix timer for node %s\n",__FUNCTION__,srcStream->st_name);
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
	DEBUG_MSG(2,"%s: Starting posixtimer for node %s. Will fire in %u ms.\n",__FUNCTION__,srcStream->st_name,srcStream->period);
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
	DEBUG_MSG(2,"%s: Canceling posix timer for node %s...\n",__FUNCTION__,srcStream->st_name);
	// Delete the timer
	ret = timer_delete(timerJob->timerID);
	if (ret < 0) {
		ERR_MSG("timer_delete failed: %s\n",strerror(errno));
	}
	DEBUG_MSG(2,"%s: posix timer for node %s canceled. Was active: %d\n",__FUNCTION__,srcStream->st_name,ret);
	// Free the timer information
	FREE(timerJob);
	srcStream->timerInfo = NULL;
}

int initLayer(void) {
	union semun cmdval;
	char buffer[20];
	unsigned long addr = 0;
	int ret = 0;

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
	INFO_MSG("Mapped the kernels shared memory at %p\n",sharedMemoryUserBase);
	if (read(fdCommunicationFile,buffer,20) < 0) {
		ERR_MSG("Cannot read sharedMemoryKernelBase: %s\n",strerror(errno));
		close(fdCommunicationFile);
		return -1;
	}
	addr = strtoul(buffer,NULL,16);
	INFO_MSG("Kernel mapped shared memory at 0x%lx\n",addr);
	sharedMemoryKernelBase = (void*)addr;
	ringBufferInit();

#ifdef CALC_SLEEP_TIME
	successfullReads = 0;
	failedReads = 0;
#endif
	sleepTime = INIT_SLEEP_TIME;
	waitingQueries = 0;
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
	DEBUG_MSG(1,"Requesting a complete snapshot of the datamodel from the kernel\n");
	do {
		ret = ringBufferWrite(txBuffer,MSG_DM_SNAPSHOT,NULL);
		if (ret == -1) {
			// Oh no. Start busy waiting...
			MSLEEP(100);
		}
	} while (ret == -1);

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
	INFO_MSG("Max amount of outstanding queries: %d\n",maxWaitingQueries);
	INFO_MSG("Missed %d timer\n", missedTimer);
}
