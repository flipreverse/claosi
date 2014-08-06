#ifndef __API_H__
#define __API_H__

#include <output.h>
#include <datamodel.h>
#include <query.h>

#ifdef __KERNEL__
#include <linux/list.h>
#else
#include <signal.h>
#include <sys/queue.h>
#endif

extern DataModelElement_t *slcDataModel;
DECLARE_LOCK_EXTERN(slcLock);

/**
 * On each call to startObjStatusThread an instance of QueryStatusJob_t will be
 * created and a pointer to it will be passed as a threads parameter.
 * The status thread uses the function pointer {@link statusFn} to generate a list of tuples for a certain object.
 * Afterwards it will enqueue each tuple and the query {@link query} for execution.
 */
typedef struct QueryStatusJob {
	/**
	 * A pointer to the recently registered query which first operator is a ObjectStream_t and has at least
	 * OBJECT_STATUS set.
	 */
	Query_t *query;
	/**
	 * A pointer to the datamodel objects status function. Stored at dm->typeInfo->status.
	 */
	generateStatus statusFn;
} QueryStatusJob_t;
/**
 * A QueryJob_t represents a job for the query execution thread.
 * Multiple instances may references the same query or the same tuple.
 * Only a query in conjunction with a tuple are unique.
 */
typedef struct QueryJob {
	/**
	 * Auxiliary member to maintain each query in a linked-list
	 */
	#ifdef __KERNEL__
	struct list_head list;
	#else
	STAILQ_ENTRY(QueryJob) listEntry;
	#endif
	/**
	 * A pointer to the query
	 */
	Query_t *query;
	/**
	 * A pointer to the tuple
	 */
	Tupel_t *tuple;
	int step;
} QueryJob_t;
/**
 * An instance of QueryTimerJob_t holds any information needed by the 
 * kernel-specific code to maintain the timer for a certain source.
 * Relation between a source and QueryTimerJob_t: 1:n .
 */
typedef struct QueryTimerJob {
	/**
	 * Easier access to the period. Needed to reschedule the hrtimer.
	 * It is accessible through query->root->period, which is more expensive.
	 */
	int period;
	/**
	 * A pointer to the query a module registered on node {@link dm}.
	 */
	Query_t *query;
	/**
	 * Access the datamodel node to acquire/release the lock and call the status function.
	 */
	DataModelElement_t *dm;
	/**
	 * The actual timer :-)
	 */
	#ifdef __KERNEL__
	struct hrtimer timer;
	#else
	timer_t timerID;
	struct sigevent timerNotify;
	struct itimerspec timerValue;
	#endif
} QueryTimerJob_t;

int registerProvider(DataModelElement_t *dm, Query_t *queries);
int unregisterProvider(DataModelElement_t *dm, Query_t *queries);
int registerQuery(Query_t *queries);
int unregisterQuery(Query_t *queries);
int initSLC(void);
void destroySLC(void);
int initSLCDatamodel(void);

void eventOccured(char *datamodelName, Tupel_t *tupel);
void objectChanged(char *datamodelName, Tupel_t *tupel, int event);

/*
 * The following function need to be implemented by the instance of a certain layer , e.g. the kernel.
 */
/**
 * Enqueues a {@link query} for execution. The layer instance has to decide when the {@link query} will be
 * apllied to the {@link tuple}.
 * But it has to ensure that it operates asynchronously!
 * @param query the query to be executed
 * @param tuple the tuple
 * @param step indicates the stage of {@link query} the execution should start with
 */
void enqueueQuery(Query_t *query, Tupel_t *tuple, int step);
/**
 * Creates and starts a thread, which calls a objects status function {@link status} and
 * and enqueues each returned tuple for execution.
 * @param query a pointer to the query 
 * @param status a pointer to the objects status function
 * @param flags All regstier* kernel functions acquire the slcLock with irqsave. To create and start it is necessary to release and re-acquire the lock again. Hence to update the flags.
 */
#ifdef __KERNEL__
void startObjStatusThread(Query_t *query, generateStatus status,unsigned long *flags);
#else
void startObjStatusThread(Query_t *query, generateStatus status);
#endif
/**
 * Setups and starts an layer-specific timer firing every query->root->period ms.
 * The layer-specific code has to call the datamodel nodes status' function and enqueue the returned tuple including the {@link query} for execution.
 * @param dm a pointer to the datamodel node representing the source {@link query} should be executed on
 * @param query a pointer to the query which should be executed query x ms
 */
void startSourceTimer(DataModelElement_t *dm, Query_t *query);
/**
 * Stops (and destroys) the layer-specific timer.
 * Each instance of SourceStream_t holds a timerInfo pointer. It points to a memory location where layer-specific information for a timer is stored.
 * Hence, this function needs a pointer to the query in order to reach the instance of SourceStream_t.
 * @param query a pointer to the query which should be executed query x ms
 */
void stopSourceTimer(Query_t *query);
/**
 * Deletes {@link query} from all lists it was enqueued to.
 * @param query a pointer to the query which should be deleted
 */
void delPendingQuery(Query_t *query);

#endif // __API_H__
