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


#endif // __API_H__
