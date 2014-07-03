#include <api.h>

DataModelElement_t *slcDataModel = NULL;
DECLARE_LOCK(slcLock);

#ifdef __KERNEL__
EXPORT_SYMBOL(slcDataModel);
EXPORT_SYMBOL(slcLock);
#endif
/**
 * Initializes the global datamodel.
 */
static int initDatamodel(void) {
	if ((slcDataModel = ALLOC(sizeof(DataModelElement_t))) == NULL) {
		return -1;
	}
	INIT_MODEL((*slcDataModel),0);
	return 0;
}

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
	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	ACQUIRE_WRITE_LOCK(slcLock);

	if (dm == NULL && queries == NULL) {
		return -EPARAM;
	}
	if (dm != NULL) {
		if ((ret = checkDataModelSyntax(slcDataModel,dm,NULL)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		// First, check if the datamodel is mergable
		if ((ret = mergeDataModel(1,slcDataModel,dm)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		// Now merge it.
		if ((ret = mergeDataModel(0,slcDataModel,dm)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
	}
	if (queries != NULL) {
		if ((ret = checkQueries(slcDataModel,queries,NULL,0)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		#ifdef __KERNEL__
		if ((ret = addQueries(slcDataModel,queries,&flags)) < 0) {
		#else
		if ((ret = addQueries(slcDataModel,queries)) < 0) {
		#endif
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
	}
	RELEASE_WRITE_LOCK(slcLock);

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
	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	ACQUIRE_WRITE_LOCK(slcLock);

	if (dm == NULL && queries == NULL) {
		RELEASE_WRITE_LOCK(slcLock);
		return -EPARAM;
	}
	if (queries != NULL) {
		if ((ret = checkQueries(slcDataModel,queries,NULL,1)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		#ifdef __KERNEL__
		if ((ret = delQueries(slcDataModel,queries,&flags)) < 0) {
		#else
		if ((ret = delQueries(slcDataModel,queries)) < 0) {
		#endif
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
	}
	if (dm != NULL) {
		if ((ret = checkDataModelSyntax(slcDataModel,dm,NULL)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		if ((ret = deleteSubtree(&slcDataModel,dm)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		// If deleteSubtree removes even the root node, it is necessary to reinitialize the global datamodel
		if (slcDataModel == NULL) {
			initDatamodel();
		}
	}
	RELEASE_WRITE_LOCK(slcLock);
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
	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	ACQUIRE_WRITE_LOCK(slcLock);

	if (queries != NULL) {
		if ((ret = checkQueries(slcDataModel,queries,NULL,1)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		#ifdef __KERNEL__
		if ((ret = addQueries(slcDataModel,queries,&flags)) < 0) {
		#else
		if ((ret = addQueries(slcDataModel,queries)) < 0) {
		#endif
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
	} else {
		RELEASE_WRITE_LOCK(slcLock);
		return -EPARAM;
	}
	RELEASE_WRITE_LOCK(slcLock);
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
	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	ACQUIRE_WRITE_LOCK(slcLock);

	if (queries != NULL) {
		if ((ret = checkQueries(slcDataModel,queries,NULL,1)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		#ifdef __KERNEL__
		if ((ret = delQueries(slcDataModel,queries,&flags)) < 0) {
		#else
		if ((ret = delQueries(slcDataModel,queries)) < 0) {
		#endif
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
	} else {
		RELEASE_WRITE_LOCK(slcLock);
		return -EPARAM;
	}
	RELEASE_WRITE_LOCK(slcLock);
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
	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	ACQUIRE_WRITE_LOCK(slcLock);

	if (tupel == NULL) {
		RELEASE_WRITE_LOCK(slcLock);
		return;
	}
	if ((dm = getDescription(slcDataModel,datamodelName)) == NULL) {
		RELEASE_WRITE_LOCK(slcLock);
		return;
	}
	if (dm->dataModelType == EVENT) {
		query = ((Event_t*)dm->typeInfo)->queries;
	} else {
		RELEASE_WRITE_LOCK(slcLock);
		return;
	}

	for (i = 0; i < MAX_QUERIES_PER_DM; i++) {
		if (query[i] != NULL) {
			DEBUG_MSG(2,"Executing query(base@%p) %d: %p\n",query,i,query[i]);
			enqueueQuery(query[i],tupel);
		}
	}
	RELEASE_WRITE_LOCK(slcLock);
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
	int i = 0;
	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	DataModelElement_t *dm = NULL;
	Query_t **query = NULL;
	ObjectStream_t *objStream = NULL;
	ACQUIRE_WRITE_LOCK(slcLock);

	if (tupel == NULL) {
		RELEASE_WRITE_LOCK(slcLock);
		return;
	}
	if ((dm = getDescription(slcDataModel,datamodelName)) == NULL) {
		RELEASE_WRITE_LOCK(slcLock);
		return;
	}
	if (dm->dataModelType == OBJECT) {
		query = ((Object_t*)dm->typeInfo)->queries;
	} else {
		RELEASE_WRITE_LOCK(slcLock);
		return;
	}

	for (i = 0; i < MAX_QUERIES_PER_DM; i++) {
		if (query[i] != NULL) {
			if (query[i]->root->type == GEN_OBJECT) {
				objStream = (ObjectStream_t*)query[i]->root;
			} else {
				ERR_MSG("Weird! This should not happen! The root operator of a query registered to an object is not of type GEN_OBJECT!\n");
				continue;
			}
			if ((objStream->objectEvents & event) == event) {
				DEBUG_MSG(3,"Executing %d-th query (base@%p) %p\n",i,query,query[i]);
				enqueueQuery(query[i],tupel);
			} else {
				DEBUG_MSG(3,"Not executing %d-th query(base@%p) %p, because the event does not match the one the query was registered for (%d != %d).\n",i,query,query[i],objStream->objectEvents,event);
			}
		}
	}
	RELEASE_WRITE_LOCK(slcLock);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(objectChanged);
#endif
/**
 * Does all common initialization stuff
 */
int initSLC(void) {
	int ret = 0;

	INIT_LOCK(slcLock);
	if ((ret = initDatamodel()) < 0) {
		return ret;
	}
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
