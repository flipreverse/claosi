#ifndef __DATAMODEL_H__
#define __DATAMODEL_H__

#include <output.h>
#include <common.h>
#include <liballoc.h>

#define MAX_QUERIES_PER_DM	8

#define ALLOC_CHILDREN_ARRAY(size)			(DataModelElement_t**)ALLOC(sizeof(DataModelElement_t*) * size)
#define ALLOC_TYPEINFO(type)				(type*)ALLOC(sizeof(type))
#define REALLOC_CHILDREN_ARRAY(ptr,size)	(DataModelElement_t**)REALLOC(ptr,sizeof(DataModelElement_t*) * size)
#define ALLOC_STATIC_CHILDREN_ARRAY(size)	(DataModelElement_t**)ALLOC(sizeof(DataModelElement_t*) * size)

#define DECLARE_ELEMENT(elem)				static DataModelElement_t elem;
#define DECLARE_ELEMENTS(vars...)			static DataModelElement_t vars;
#define ADD_CHILD(node,slot,child)			node.children[slot] = &child;

enum DataModelType {
	MODEL			=	0x0,
	NAMESPACE		=	0x1,
	SOURCE			=	0x2,
	EVENT			=	0x4,
	OBJECT			=	0x8,
	TYPE			=	0x10,
	REF				=	0x20,
	STRING			=	0x40,
	INT				=	0x80,
	FLOAT			=	0x100,
	COMPLEX			=	0x200,
	ARRAY			=	0x400,
	BYTE			=	0x800
};

#if defined(__LP64__) && __LP64__ == 1
	#define PTR_TYPE	long long
#else
	#define PTR_TYPE	int
#endif

// Size of the plain datatypes in the datamodel
enum SizePOD {
	SIZE_INT		=	sizeof(int),			// :-)
	SIZE_STRING		=	sizeof(PTR_TYPE),		// In fact, a STRING is just a pointer to a char array. Thus, its size is equal to the size of a pointer, which is 4 bytes on ARM.
	SIZE_FLOAT		=	sizeof(double),			// Assuming a floating point number with double precision
	SIZE_BYTE		=	sizeof(char),			// Nothing more to say. :-)
	SIZE_REF		=	0x4,					// TODO
	SIZE_ARRAY		=	sizeof(PTR_TYPE)		// An array is just a pointer to large amount of memory where the actual data is stored. The first four bytes of an array contain an int holding the array length.
};

enum {
	ZERO		=	0x0,
	GRE_ZERO	=	0x1,
	GEQ_ZERO	=	0x2
};

typedef struct DataModelElement{
	DECLARE_BUFFER(name);
	struct DataModelElement *parent;
	int	childrenLen;
	struct DataModelElement **children;
	unsigned int dataModelType;
	void *typeInfo;
} DataModelElement_t;

typedef struct TypeItem {
	unsigned short type;
	DECLARE_BUFFER(name);
} TypeItem_t;

typedef void (*activateEventCallback)(void);
typedef void (*deactivateEventCallback)(void);

struct Query;
typedef struct Tupel Tupel_t;

typedef struct Event {
	unsigned short returnType;
	DECLARE_BUFFER(returnName);
	activateEventCallback activate;
	deactivateEventCallback deactivate;
	struct Query *queries[MAX_QUERIES_PER_DM];
	int numQueries;
} Event_t;

typedef Tupel_t* (*getSource)(void);

typedef struct Source {
	unsigned short returnType;
	DECLARE_BUFFER(returnName);
	getSource callback;
	struct Query *queries[MAX_QUERIES_PER_DM];
	int numQueries;
	DECLARE_LOCK(lock);
} Source_t;

typedef struct Tupel Tupel_t;

typedef void (*activateObject)(void);
typedef void (*deactivateObject)(void);
typedef Tupel_t* (*generateStatus)(void);

typedef struct Object {
	unsigned short identifierType;
	activateObject activate;
	deactivateObject deactivate;
	generateStatus status;
	struct Query *queries[MAX_QUERIES_PER_DM];
	int numQueries;
} Object_t;

void printDatamodel(DataModelElement_t *root);
int checkDataModelSyntax(DataModelElement_t *rootCurrent,DataModelElement_t *rootToCheck, DataModelElement_t **errElem);
DataModelElement_t* getDescription(DataModelElement_t *root, char *name);
int mergeDataModel(int justCheckSyntax, DataModelElement_t *oldTree, DataModelElement_t *newTree) ;
void freeDataModel(DataModelElement_t *node, int freeNodes);
DataModelElement_t* copySubtree(DataModelElement_t *rootOrigin);
int deleteSubtree(DataModelElement_t **root, DataModelElement_t *tree);
int getOffset(DataModelElement_t *parent, char *child);
void freeNode(DataModelElement_t *node, int freeNodes);
int getDataModelSize(DataModelElement_t *rootDM, DataModelElement_t *elem, int ignoreArray);
int calcDatamodelSize(DataModelElement_t *node);
void copyAndCollectDatamodel(DataModelElement_t *node, void *freeMem);
void rewriteDatamodelAddress(DataModelElement_t *node, void *oldBaseAddr, void *newBaseAddr);
#define getSize(rootDMVar, elemVar) getDataModelSize(rootDMVar,elemVar,1)

#define SET_CHILDREN_ARRAY(varName,numChildren) if (numChildren > 0) { \
		varName.children = ALLOC_STATIC_CHILDREN_ARRAY(numChildren); \
	} else { \
		varName.children = NULL; \
	}

#define INIT_QUERY_ARRAY(queryVar,size) 	for (i = 0; i < size; i++) { \
	queryVar[i] = NULL; \
}

#define INIT_MODEL(varName,numChildren)	memset(&varName.name,'\0',MAX_NAME_LEN); \
	varName.childrenLen = numChildren; \
	SET_CHILDREN_ARRAY(varName,numChildren) \
	varName.dataModelType = MODEL; \
	varName.typeInfo = NULL; \
	varName.parent = NULL;

#define INIT_NS(varName,nodeName,parentNode,numChildren)	strncpy((char*)&varName.name,nodeName,MAX_NAME_LEN); \
	varName.childrenLen = numChildren; \
	SET_CHILDREN_ARRAY(varName,numChildren) \
	varName.dataModelType = NAMESPACE; \
	varName.typeInfo = NULL; \
	varName.parent = &parentNode;

#define INIT_SOURCE_BASIC(varName,nodeName,parentNode,cbFunc)	strncpy((char*)&varName.name,nodeName,MAX_NAME_LEN); \
	varName.childrenLen = 0; \
	varName.children = NULL; \
	varName.parent = &parentNode; \
	varName.dataModelType = SOURCE; \
	varName.typeInfo = ALLOC(sizeof(Source_t)); \
	((Source_t*)varName.typeInfo)->callback = cbFunc; \
	((Source_t*)varName.typeInfo)->numQueries = 0; \
	INIT_QUERY_ARRAY(((Source_t*)varName.typeInfo)->queries,MAX_QUERIES_PER_DM);

#define INIT_SOURCE_POD(varName,nodeName,parentNode,srcType,cbFunc)	INIT_SOURCE_BASIC(varName,nodeName,parentNode,cbFunc) \
	memset(&((Source_t*)varName.typeInfo)->returnName,0,MAX_NAME_LEN); \
	((Source_t*)varName.typeInfo)->returnType = srcType;

#define INIT_SOURCE_COMPLEX(varName,nodeName,parentNode,returnTypeName,cbFunc)	INIT_SOURCE_BASIC(varName,nodeName,parentNode,cbFunc) \
	strncpy((char*)&((Source_t*)varName.typeInfo)->returnName,returnTypeName,MAX_NAME_LEN); \
	((Source_t*)varName.typeInfo)->returnType = COMPLEX;

#define INIT_OBJECT(varName,nodeName,parentNode,numChildren,idType,activateFunc, deactivateFunc, statusFunc)	strncpy((char*)&varName.name,nodeName,MAX_NAME_LEN); \
	varName.childrenLen = numChildren; \
	SET_CHILDREN_ARRAY(varName,numChildren) \
	varName.parent = &parentNode; \
	varName.dataModelType = OBJECT; \
	varName.typeInfo = ALLOC(sizeof(Object_t)); \
	((Object_t*)varName.typeInfo)->identifierType = idType; \
	((Object_t*)varName.typeInfo)->activate = activateFunc; \
	((Object_t*)varName.typeInfo)->deactivate = deactivateFunc; \
	((Object_t*)varName.typeInfo)->status = statusFunc; \
	((Object_t*)varName.typeInfo)->numQueries = 0; \
	INIT_QUERY_ARRAY(((Object_t*)varName.typeInfo)->queries,MAX_QUERIES_PER_DM);

#define INIT_EVENT_POD(varName,nodeName,parentNode,evtType,regFunc, unregFunc)	strncpy((char*)&varName.name,nodeName,MAX_NAME_LEN); \
	varName.childrenLen = 0; \
	varName.children = NULL; \
	varName.parent = &parentNode; \
	varName.dataModelType = EVENT; \
	varName.typeInfo = ALLOC(sizeof(Event_t)); \
	((Event_t*)varName.typeInfo)->returnType = evtType; \
	memset(&((Event_t*)varName.typeInfo)->returnName,0,MAX_NAME_LEN); \
	((Event_t*)varName.typeInfo)->activate = regFunc; \
	((Event_t*)varName.typeInfo)->deactivate = unregFunc; \
	((Event_t*)varName.typeInfo)->numQueries = 0; \
	INIT_QUERY_ARRAY(((Event_t*)varName.typeInfo)->queries,MAX_QUERIES_PER_DM)

#define INIT_EVENT_COMPLEX(varName,nodeName,parentNode,returnTypeName,regFunc, unregFunc)	strncpy((char*)&varName.name,nodeName,MAX_NAME_LEN); \
	varName.childrenLen = 0; \
	varName.children = NULL; \
	varName.parent = &parentNode; \
	varName.dataModelType = EVENT; \
	varName.typeInfo = ALLOC(sizeof(Event_t)); \
	((Event_t*)varName.typeInfo)->returnType = COMPLEX; \
	strncpy((char*)&((Event_t*)varName.typeInfo)->returnName,returnTypeName,MAX_NAME_LEN); \
	((Event_t*)varName.typeInfo)->activate = regFunc; \
	((Event_t*)varName.typeInfo)->deactivate = unregFunc; \
	((Event_t*)varName.typeInfo)->numQueries = 0; \
	INIT_QUERY_ARRAY(((Event_t*)varName.typeInfo)->queries,MAX_QUERIES_PER_DM)

#define INIT_TYPE(varName,nodeName,parentNode,numChildren)	strncpy((char*)&varName.name,nodeName,MAX_NAME_LEN);\
	varName.childrenLen = numChildren; \
	SET_CHILDREN_ARRAY(varName,numChildren) \
	varName.parent = &parentNode; \
	varName.dataModelType = TYPE; \
	varName.typeInfo = NULL;

#define INIT_PLAINTYPE(varName,nodeName,parentNode,type)	strncpy((char*)&varName.name,nodeName,MAX_NAME_LEN);\
	varName.childrenLen = 0; \
	varName.children = NULL; \
	varName.parent = &parentNode; \
	varName.dataModelType = type; \
	varName.typeInfo = NULL;
	
#define INIT_REF(varName,nodeName,parentNode,refName)	strncpy((char*)&varName.name,nodeName,MAX_NAME_LEN); \
	varName.childrenLen = 0; \
	varName.children = NULL; \
	varName.parent = &parentNode; \
	varName.dataModelType = REF; \
	varName.typeInfo = ALLOC(sizeof(char) * (strlen(refName) + 1)); \
	strcpy((char*)varName.typeInfo,refName);

#endif // __DATAMODEL_H__
