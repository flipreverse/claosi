#ifndef __API_H__
#define __API_H__

#include <datamodel.h>
#include <query.h>

int registerProvider(DataModelElement_t *dm, Query_t *queries);
int unregisterProvider(DataModelElement_t *dm, Query_t *queries);
int registerQuery(Query_t *queries);
int unregisterQuery(Query_t *queries);
int initSLC(void);
void destroySLC(void);

extern DataModelElement_t *slcDataModel;
DECLARE_LOCK_EXTERN(slcLock);

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
 */
void enqueueQuery(Query_t *query, Tupel_t *tuple);
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

#endif // __API_H__
