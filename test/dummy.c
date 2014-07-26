#include <api.h>

void enqueueQuery(Query_t *query, Tupel_t *tuple) {
	executeQuery(SLC_DATA_MODEL, query, &tuple);
}

void startObjStatusThread(Query_t *query, generateStatus statusFn) {
	
}

void startSourceTimer(DataModelElement_t *dm, Query_t *query) {
	
}

void stopSourceTimer(Query_t *query) {
	
}
void acquireSlcLock(void) {
	
}

void releaseSlcLock(void) {
	
}
