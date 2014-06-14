#ifndef __API_H__
#define __API_H__

#include <datamodel.h>
#include <query.h>

int registerProvider(DataModelElement_t *dm, Query_t *queries);
int unregisterProvider(DataModelElement_t *dm, Query_t *queries);
int registerQuery(Query_t *queries);
int unregisterQuery(Query_t *queries);

extern DataModelElement_t *slcDataModel;

void eventOccured(char *datamodelName, Tupel_t *tupel);
void objectChanged(char *datamodelName, Tupel_t *tupel);

#endif // __API_H__
