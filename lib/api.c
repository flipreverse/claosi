#include <api.h>

DataModelElement_t *SLC_DATA_MODEL = NULL;
DECLARE_LOCK(slcLock);

#ifdef __KERNEL__
EXPORT_SYMBOL(slcDataModel);
EXPORT_SYMBOL(slcLock);
#endif
/**
 * Initializes the global datamodel.
 */
int initSLCDatamodel(void) {
	SLC_DATA_MODEL = ALLOC(sizeof(DataModelElement_t));
	if (SLC_DATA_MODEL == NULL) {
		return -1;
	}
	INIT_MODEL((*SLC_DATA_MODEL),0);
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
		if ((ret = checkDataModelSyntax(SLC_DATA_MODEL,dm,NULL)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		// First, check if the datamodel is mergable
		if ((ret = mergeDataModel(1,SLC_DATA_MODEL,dm)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		sendDatamodel(dm,1);
		// Now merge it.
		if ((ret = mergeDataModel(0,SLC_DATA_MODEL,dm)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
	}
	if (queries != NULL) {
		if ((ret = checkQueries(SLC_DATA_MODEL,queries,NULL)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		#ifdef __KERNEL__
		if ((ret = addQueries(SLC_DATA_MODEL,queries,&flags)) < 0) {
		#else
		if ((ret = addQueries(SLC_DATA_MODEL,queries)) < 0) {
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
		if ((ret = checkQueries(SLC_DATA_MODEL,queries,NULL)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		#ifdef __KERNEL__
		if ((ret = delQueries(SLC_DATA_MODEL,queries,&flags)) < 0) {
		#else
		if ((ret = delQueries(SLC_DATA_MODEL,queries)) < 0) {
		#endif
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
	}
	if (dm != NULL) {
		if ((ret = checkDataModelSyntax(SLC_DATA_MODEL,dm,NULL)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		if ((ret = deleteSubtree(&SLC_DATA_MODEL,dm)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		sendDatamodel(dm,0);
		// If deleteSubtree removes even the root node, it is necessary to reinitialize the global datamodel
		if (SLC_DATA_MODEL == NULL) {
			initSLCDatamodel();
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
		if ((ret = checkQueries(SLC_DATA_MODEL,queries,NULL)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		#ifdef __KERNEL__
		if ((ret = addQueries(SLC_DATA_MODEL,queries,&flags)) < 0) {
		#else
		if ((ret = addQueries(SLC_DATA_MODEL,queries)) < 0) {
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
		if ((ret = checkQueries(SLC_DATA_MODEL,queries,NULL)) < 0) {
			RELEASE_WRITE_LOCK(slcLock);
			return ret;
		}
		#ifdef __KERNEL__
		if ((ret = delQueries(SLC_DATA_MODEL,queries,&flags)) < 0) {
		#else
		if ((ret = delQueries(SLC_DATA_MODEL,queries)) < 0) {
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
 * Informs the slc that there is a new {@link tuple} for {@link query}.
 * {@link query} is registered to an event.
 * @param query a pointer to the query which is registered to the caller
 * @param tuple a pointer to the newly created tuple
 */
void eventOccuredUnicast(Query_t *query, Tupel_t *tuple) {
	/*	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	ACQUIRE_READ_LOCK(slcLock);*/

	if (tuple == NULL || query == NULL) {
//		RELEASE_READ_LOCK(slcLock);
		return;
	}
	// for now, just forward the tuple as well as the query to the layer-specific code and enqueue it for execution
	enqueueQuery(query,tuple,0);
//	RELEASE_READ_LOCK(slcLock);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(eventOccuredUnicast);
#endif
/**
 * Notifies the slc about a recently occured event
 * The caller has to ensure that all items noted in itemLen are allocated and initialized. Furthermore
 * he must set and init all values in an item, e.g. init an array.
 * @param datamodelName the path to the event, e.g. net.device.onTx
 * @param tupel the tupel
 */
void eventOccuredBroadcast(char *datamodelName, Tupel_t *tuple) {
	DataModelElement_t *dm = NULL;
	Tupel_t *copyTuple = NULL, *curTuple = NULL;
	Query_t **queries = NULL;
	int i = 0, j = 0, numQueries = 0;
/*	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	ACQUIRE_READ_LOCK(slcLock);*/

	if (tuple == NULL) {
//		RELEASE_READ_LOCK(slcLock);
		return;
	}
	dm = getDescription(SLC_DATA_MODEL,datamodelName);
	if (dm == NULL) {
		freeTupel(SLC_DATA_MODEL,tuple);
//		RELEASE_READ_LOCK(slcLock);
		return;
	}
	if (dm->dataModelType == EVENT) {
		queries = ((Event_t*)dm->typeInfo)->queries;
		numQueries = ((Event_t*)dm->typeInfo)->numQueries;
	} else {
		freeTupel(SLC_DATA_MODEL,tuple);
//		RELEASE_READ_LOCK(slcLock);
		return;
	}

	curTuple = tuple;
	for (i = 0; i < MAX_QUERIES_PER_DM; i++) {
		if (queries[i] != NULL) {
			if (j + 1 < numQueries) {
				copyTuple = copyTupel(SLC_DATA_MODEL,curTuple);
				if (copyTuple == NULL) {
					ERR_MSG("Cannot copy tuple!\n");
					freeTupel(SLC_DATA_MODEL,curTuple);
//					RELEASE_READ_LOCK(slcLock);
					return;
				}
				j++;
			} else {
				copyTuple = NULL;
			}
			DEBUG_MSG(2,"Executing query(base@%p) %d: %p\n",queries,i,queries[i]);
			enqueueQuery(queries[i],curTuple,0);
			curTuple = copyTuple;
		}
	}
//	RELEASE_READ_LOCK(slcLock);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(eventOccuredBroadcast);
#endif
/**
 * Notifies the slc about a recently chaned object.
 * The caller has to ensure that all items noted in itemLen are allocated and initialized. Furthermore
 * he must set and init all values in an item, e.g. init an array.
 * @param datamodelName the path to the event, e.g. net.device.onTx
 * @param tupel the tupel
 * @param event a bitmask describing the event type
 */
void objectChangedBroadcast(char *datamodelName, Tupel_t *tuple, int event) {
	int i = 0, j = 0, numQueries;
/*	#ifdef __KERNEL__
	unsigned long flags;
	#endif*/
	DataModelElement_t *dm = NULL;
	Query_t **queries = NULL;
	ObjectStream_t *objStream = NULL;
	Tupel_t *copyTuple = NULL, *curTuple = NULL;

//	ACQUIRE_READ_LOCK(slcLock);

	if (tuple == NULL) {
//		RELEASE_READ_LOCK(slcLock);
		return;
	}
	dm = getDescription(SLC_DATA_MODEL,datamodelName);
	if (dm == NULL) {
		freeTupel(SLC_DATA_MODEL,tuple);
//		RELEASE_READ_LOCK(slcLock);
		return;
	}
	if (dm->dataModelType == OBJECT) {
		queries = ((Object_t*)dm->typeInfo)->queries;
		numQueries = ((Object_t*)dm->typeInfo)->numQueries;
	} else {
		freeTupel(SLC_DATA_MODEL,tuple);
//		RELEASE_READ_LOCK(slcLock);
		return;
	}

	curTuple = tuple;
	for (i = 0; i < MAX_QUERIES_PER_DM; i++) {
		if (queries[i] != NULL) {
			if (queries[i]->root->type == GEN_OBJECT) {
				objStream = (ObjectStream_t*)queries[i]->root;
			} else {
				ERR_MSG("Weird! This should not happen! The root operator of a query registered to an object is not of type GEN_OBJECT!\n");
				continue;
			}
			if ((objStream->objectEvents & event) == event) {
				if (j + 1 < numQueries) {
					copyTuple = copyTupel(SLC_DATA_MODEL,curTuple);
					if (copyTuple == NULL) {
						ERR_MSG("Cannot copy tuple!\n");
						freeTupel(SLC_DATA_MODEL,curTuple);
	//					RELEASE_READ_LOCK(slcLock);
						return;
					}
					j++;
				} else {
					copyTuple = NULL;
				}
				DEBUG_MSG(3,"Executing %d-th query (base@%p) %p\n",i,queries,queries[i]);
				enqueueQuery(queries[i],curTuple,0);
				curTuple = copyTuple;
			} else {
				DEBUG_MSG(3,"Not executing %d-th query(base@%p) %p, because the event does not match the one the query was registered for (%d != %d).\n",i,queries,queries[i],objStream->objectEvents,event);
			}
		}
	}
//	RELEASE_READ_LOCK(slcLock);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(objectChangedBroadcast);
#endif
/**
 * Informs the slc that there is a new {@link tuple} for {@link query}.
 * {@link query} is registered to an object.
 * @param query a pointer to the query which is registered to the caller
 * @param tuple a pointer to the newly created tuple
 */
void objectChangedUnicast(Query_t *query, Tupel_t *tuple) {
	/*	#ifdef __KERNEL__
	unsigned long flags;
	#endif
	ACQUIRE_READ_LOCK(slcLock);*/

	if (tuple == NULL || query == NULL) {
//		RELEASE_READ_LOCK(slcLock);
		return;
	}
	// for now, just forward the tuple as well as the query to the layer-specific code and enqueue it for execution
	enqueueQuery(query,tuple,0);
//	RELEASE_READ_LOCK(slcLock);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(objectChangedUnicast);
#endif

/**
 * Does all common initialization stuff
 */
int initSLC(void) {
	int ret = 0;

	INIT_LOCK(slcLock);
	if ((ret = initSLCDatamodel()) < 0) {
		return ret;
	}
	return 0;
}
/**
 * Cleans all up
 */
void destroySLC(void) {
	if (SLC_DATA_MODEL != NULL) {
		FREE(SLC_DATA_MODEL);
	}
}
