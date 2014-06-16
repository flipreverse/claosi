#include <api.h>

DataModelElement_t *slcDataModel = NULL;
#ifdef __KERNEL__
EXPORT_SYMBOL(slcDataModel);
#endif
/**
 * Tries to register a new datamodel {@link dm} and {@link queries}.
 * First, it checks, if {@link dm}s syntax is correct and if it is mergeable. If so, it will be merged
 * in the slc datamodel.
 * If present, the syntax of {@link queries} is checked. If so, the queries will be added and a id will be assigned.
 * @param dm the proposed datamodel
 * @param queries the queries
 * @return 0 on sucess. A value less than zero on error. The value indicates the type of error.
 */
int registerProvider(DataModelElement_t *dm, Query_t *queries) {
	int ret = 0;

	if (dm == NULL && queries == NULL) {
		return -EPARAM;
	}
	if (dm != NULL) {
		if ((ret = checkDataModelSyntax(slcDataModel,dm,NULL)) < 0) {
			return ret;
		}
		// First, check if the datamodel is mergable
		if ((ret = mergeDataModel(1,slcDataModel,dm)) < 0) {
			return ret;
		}
		// Now merge it.
		if ((ret = mergeDataModel(0,slcDataModel,dm)) < 0) {
			return ret;
		}
	}
	if (queries != NULL) {
		if ((ret = checkQueries(slcDataModel,queries,NULL,0)) < 0) {
			return ret;
		}
		if ((ret = addQueries(slcDataModel,queries)) < 0) {
			return ret;
		}
	}

	return 0;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(registerProvider);
#endif
/**
 * Tries to unregister the datamodel {@link dm} and {@link queries}.
 * First, it removes all queries. Second, it tries to remove the datamodel.
 * This could fail, because there are some queries left, which are registered to even these nodes.
 * @param dm the proposed datamodel
 * @param queries the 
 * @return 0 on sucess. A value less than zero on error. The value indicates the type of error.
 */
int unregisterProvider(DataModelElement_t *dm, Query_t *queries) {
	int ret = 0;

	if (dm == NULL && queries == NULL) {
		return -EPARAM;
	}
	if (queries != NULL) {
		if ((ret = checkQueries(slcDataModel,queries,NULL,1)) < 0) {
			return ret;
		}
		if ((ret = delQueries(slcDataModel,queries)) < 0) {
			return ret;
		}
	} else {
		return -EPARAM;
	}
	if (dm != NULL) {
		if ((ret = checkDataModelSyntax(slcDataModel,dm,NULL)) < 0) {
			return ret;
		}
		if ((ret = deleteSubtree(&slcDataModel,dm)) < 0) {
			return ret;
		}
		if (slcDataModel == NULL) {
			initSLC();
		}
	}
	return 0;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(unregisterProvider);
#endif
/**
 * Tries to register one or more queries. Checks its synatx and registers them to the corresponding nodes.
 * @param queries the queries
 * @return 0 on sucess. A value less than zero on error. The value indicates the type of error.
 */
int registerQuery(Query_t *queries) {
	int ret = 0;

	if (queries != NULL) {
		if ((ret = checkQueries(slcDataModel,queries,NULL,1)) < 0) {
			return ret;
		}
		if ((ret = addQueries(slcDataModel,queries)) < 0) {
			return ret;
		}
	} else {
		return -EPARAM;
	}
	return 0;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(registerQuery);
#endif
/**
 * Tries to unregister one or more queries. Checks its synatx and unregisters them from the corresponding nodes.
 * @param queries the queries
 * @return 0 on sucess. A value less than zero on error. The value indicates the type of error.
 */
int unregisterQuery(Query_t *queries) {
	int ret = 0;

	if (queries != NULL) {
		if ((ret = checkQueries(slcDataModel,queries,NULL,1)) < 0) {
			return ret;
		}
		if ((ret = delQueries(slcDataModel,queries)) < 0) {
			return ret;
		}
	} else {
		return -EPARAM;
	}
	return 0;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(unregisterQuery);
#endif
/**
 * Notifies the slc about a recently occured event
 * The caller has to ensure that all items noted in itemLen are allocated and initialized. Furthermore
 * he must set and init all values in an item, e.g. init an array.
 * @param datamodelName the path to the event, e.g. net.device.onTx
 * @param tupel the tupel
 */
void eventOccured(char *datamodelName, Tupel_t *tupel) {
	DataModelElement_t *dm = NULL;
	Query_t **query = NULL;
	int i = 0;

	if (tupel == NULL) {
		return;
	}
	if ((dm = getDescription(slcDataModel,datamodelName)) == NULL) {
		return;
	}
	if (dm->dataModelType == EVENT) {
		query = ((Event_t*)dm->typeInfo)->queries;
	} else {
		return;
	}

	for (i = 0; i < MAX_QUERIES_PER_DM; i++) {
		if (query[i] != NULL) {
			DEBUG_MSG(2,"Executing query(base@%p) %d: %p\n",query,i,query[i]);
			executeQuery(slcDataModel,query[i],&tupel);
		}
	}
}
#ifdef __KERNEL__
EXPORT_SYMBOL(eventOccured);
#endif
/**
 * Notifies the slc about a recently chaned object.
 * The caller has to ensure that all items noted in itemLen are allocated and initialized. Furthermore
 * he must set and init all values in an item, e.g. init an array.
 * @param datamodelName the path to the event, e.g. net.device.onTx
 * @param tupel the tupel
 * @param event a bitmask describing the event type
 */
void objectChanged(char *datamodelName, Tupel_t *tupel, int event) {
	DataModelElement_t *dm = NULL;
	Query_t **query = NULL;
	int i = 0;

	if (tupel == NULL) {
		return;
	}
	if ((dm = getDescription(slcDataModel,datamodelName)) == NULL) {
		return;
	}
	if (dm->dataModelType == OBJECT) {
		query = ((Object_t*)dm->typeInfo)->queries;
	} else {
		return;
	}

	for (i = 0; i < MAX_QUERIES_PER_DM; i++) {
		if (query[i] != NULL) {
			executeQuery(slcDataModel,query[i],&tupel);
		}
	}
}
#ifdef __KERNEL__
EXPORT_SYMBOL(objectChanged);
#endif
/**
 * Does all common initialization stuff
 */
int initSLC(void) {
	if ((slcDataModel = ALLOC(sizeof(DataModelElement_t))) == NULL) {
		return -1;
	}
	INIT_MODEL((*slcDataModel),0);
	return 0;
}
/**
 * Cleans all up
 */
void destroySLC(void) {
	if (slcDataModel != NULL) {
		FREE(slcDataModel);
	}
}
