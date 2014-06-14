#include <api.h>

DataModelElement_t *slcDataModel = NULL;
#ifdef __KERNEL__
EXPORT_SYMBOL(slcDataModel);
#endif

int registerProvider(DataModelElement_t *dm, Query_t *queries) {
	int ret = 0;

	if (dm == NULL && queries == NULL) {
		return -EPARAM;
	}
	if (dm != NULL) {
		if ((ret = checkDataModelSyntax(slcDataModel,dm,NULL)) < 0) {
			return ret;
		}
		if ((ret = mergeDataModel(1,slcDataModel,dm)) < 0) {
			return ret;
		}
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
			executeQuery(slcDataModel,query[i],&tupel);
		}
	}
}
#ifdef __KERNEL__
EXPORT_SYMBOL(eventOccured);
#endif

void objectChanged(char *datamodelName, Tupel_t *tupel) {
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

int initSLC(void) {
	if ((slcDataModel = ALLOC(sizeof(DataModelElement_t))) == NULL) {
		return -1;
	}
	INIT_MODEL((*slcDataModel),0);
	return 0;
}

void destroySLC(void) {
	if (slcDataModel != NULL) {
		FREE(slcDataModel);
	}
}
