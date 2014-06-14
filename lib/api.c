#include <api.h>

DataModelElement_t *slcDataModel = NULL;

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
			
		}
	}
	return 0;
}

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
