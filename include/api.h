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

#endif // __API_H__
