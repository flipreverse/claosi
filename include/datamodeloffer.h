#ifndef __DATAMODELOFFER_H__
#define __DATAMODELOFFER_H__

#include <datamodel.h>

typedef void (*queryDataModelElement)(DataModelElement_t*);

struct DataModelOffer {
	queryDataModelElement queryPtr;
	DataModelElement_t *newDataModel;
	struct DataModelOffer *next;
} DataModelOffer_t;

#endif // __DATAMODELOFFER_H__
