#ifndef __QUERY_H__
#define __QUERY_H__

#include <output.h>
#include <common.h>
#include <datamodel.h>
#include <resultset.h>

#define GET_BASE(varName)	(Operator_t*)&varName
#define ADD_PREDICATE(varOperator,slot,predicateVar)	varOperator.predicates[slot] = &predicateVar;
#define ADD_ELEMENT(varOperator,slot,elementVar,elementName)	varOperator.elements[slot] = &elementVar; \
	strncpy((char*)&elementVar.name,elementName,MAX_NAME_LEN);

#define GET_SELECTORS(varName)			((GenStream_t*)varName->root)->selectors
#define GET_SELECTORS_LEN(varName)			((GenStream_t*)varName->root)->selectorsLen
#define SET_SELECTOR_STRING(varOperator,slot,selectorValue)	strncpy((char*)&varOperator.st_selectors[slot].value,selectorValue,MAX_NAME_LEN);

#define SET_SELECTOR_INT(varOperator,slot,selectorValue)	*(int*)(&varOperator.st_selectors[slot].value) =selectorValue;

#define INIT_SRC_STREAM(varName,srcName,numSelectors,isUrgent,childVar,srcPeriod)	strncpy((char*)&varName.st_name,srcName,MAX_NAME_LEN); \
	varName.st_type = GEN_SOURCE; \
	varName.st_child = childVar; \
	varName.st_urgent = isUrgent; \
	varName.period = srcPeriod; \
	varName.st_selectorsLen = numSelectors; \
	if (varName.st_selectorsLen > 0) { \
		varName.st_selectors = ALLOC(sizeof(Selector_t) * varName.st_selectorsLen); \
	} else { \
		varName.st_selectors = NULL; \
	} \
	varName.timerInfo = NULL;

#define INIT_OBJ_STREAM(varName,objName,numSelectors,isUrgent,childVar,listObjEvents)	strncpy((char*)&varName.st_name,objName,MAX_NAME_LEN); \
	varName.st_type = GEN_OBJECT; \
	varName.st_child = childVar; \
	varName.st_urgent = isUrgent; \
	varName.st_selectorsLen = numSelectors; \
	if (varName.st_selectorsLen > 0) { \
		varName.st_selectors = ALLOC(sizeof(Selector_t) * varName.st_selectorsLen); \
	} else { \
		varName.st_selectors = NULL; \
	} \
	varName.objectEvents = listObjEvents;

#define INIT_EVT_STREAM(varName,evtName,numSelectors,isUrgent,childVar)	strncpy((char*)&varName.st_name,evtName,MAX_NAME_LEN); \
	varName.st_type = GEN_EVENT; \
	varName.st_child = childVar; \
	varName.st_selectorsLen = numSelectors; \
	if (varName.st_selectorsLen > 0) { \
		varName.st_selectors = ALLOC(sizeof(Selector_t) * varName.st_selectorsLen); \
	} else { \
		varName.st_selectors = NULL; \
	} \
	varName.st_urgent = isUrgent;

#define INIT_JOIN(varName,joinElement,childVar,numPredicates)	strncpy((char*)&varName.element.name,joinElement,MAX_NAME_LEN); \
	varName.op_type = JOIN; \
	varName.op_child = childVar; \
	varName.predicateLen = numPredicates; \
	varName.predicates = (Predicate_t**)ALLOC(sizeof(Predicate_t*) * numPredicates);

#define INIT_FILTER(varName,childVar,numPredicates)	varName.op_type = FILTER; \
	varName.op_child = childVar; \
	varName.predicateLen = numPredicates; \
	varName.predicates = (Predicate_t**)ALLOC(sizeof(Predicate_t*) * numPredicates);

#define INIT_SELECT(varName,childVar,numElements) varName.op_type = SELECT; \
	varName.op_child = childVar; \
	varName.elementsLen = numElements; \
	varName.elements = (Element_t**)ALLOC(sizeof(Element_t**) * numElements);

#define SET_PREDICATE(varName, predType, predLeftType, predLeftName, predRightType, predRightName)	varName.type = predType; \
	varName.left.type = predLeftType; \
	strncpy((char*)&varName.left.value,predLeftName,MAX_NAME_LEN); \
	varName.right.type = predRightType; \
	strncpy((char*)&varName.right.value,predRightName,MAX_NAME_LEN);

enum QueryFlags {
	COMPACT		=	0x1,
	TRANSFERED	=	0x2
};

enum OperatorType {
	GEN_EVENT		=	0x0,
	GEN_SOURCE		=	0x1,
	GEN_OBJECT		=	0x2,
	SELECT			=	0x4,
	FILTER			=	0x8,
	SORT			=	0x10,
	GROUP			=	0x20,
	JOIN			=	0x40,
	MIN				=	0x80,
	AVG				=	0x100,
	MAX				=	0x200
};

enum ObjectEvents {
	OBJECT_CREATE	=	0x1,
	OBJECT_DELETE	=	0x2,
	OBJECT_STATUS	=	0x4
};

enum PredicateType {
	EQUAL			=	0x0,
	NEQ,
	LE,
	LEQ,
	GE,
	GEQ,
	PREDICATETYPE_END
};

enum OperandType {
	OP_STREAM			=	0x0,
	OP_POD,
	OP_JOIN,
	OPERANDTYPE_END
};

enum SizeUnit {
	EVENTS			=	0x0,
	TIME_MS			=	0x1,
	TIME_SEC		=	0x2,
	SIZEUNIT_END
};

typedef void (*queryCompletedFunction)(unsigned int,Tupel_t*);
/**
 * An object, event or source someone registers on may be nested into
 * severeal objects. Hence, the data provider must be aware for which instance of parent objects 
 * someone registered on a object, event or source.
 * That's the reason why each GenStream_t or Join which points to such a datasource needs number of parent
 * objects selectors attached to it.
 * Each selector holds a string describing in which instance of an object the caller is interested in.
 * Imagine the following example:
 * model {
 * 		obj process[int] {
 * 			src utime;
 * 		}
 * }
 * 
 * A caller wants to know the utime of a certain process. He creates a SourceStream_t having on Selector_t.
 * Its value is the process id of the process he wants to observe.
 */
typedef struct Selector {
	DECLARE_BUFFER(value);
} Selector_t;
/**
 * Identifies a query across different layers
 */
typedef struct __attribute__((packed)) QueryID {
	/**
	 * Contains the path to the node within the datamodel which is the root (a.k.a. soruce)
	 * of the query
	 */
	DECLARE_BUFFER(name);
	/**
	 * The global id of the query (assigned by addQueries())
	 */
	unsigned short id;
} QueryID_t;
/**
 * Represents the status of query.
 * It is used to handover the execution of a query to the other layer.
 */
typedef struct __attribute__((packed)) QueryContinue {
	/**
	 * Identifies the query the remote layer should continue to process.
	 * It is not possible to pass a pointer. Hence, an abstraction is needed.
	 */
	QueryID_t qID;
	/**
	 * The number of operators the remote layer has to skip before continuing execution.
	 */
	unsigned short steps;
} QueryContinue_t;

/**
 * Baseclass for a query. Each element of a query uses this struct.
 */
typedef struct __attribute__((packed)) Operator {
	unsigned short type;
	struct Operator *child;	
} Operator_t;
/**
 * Used to describe the left-hand and right-hand side of a predicate
 */
typedef struct __attribute__((packed)) Operand {
	unsigned short type;
	DECLARE_BUFFER(value)
} Operand_t;
/**
 * Describes a single element of the datamodel.
 * For now, it just contains a string holding the complete path to the desired element.
 */
typedef struct __attribute__((packed)) Element {
	DECLARE_BUFFER(name)
} Element_t;

typedef struct __attribute__((packed)) GenStream {
	Operator_t base;
	#define op_type	base.type
	#define op_child	base.child
	char urgent;
	DECLARE_BUFFER(name)
	Selector_t *selectors;
	int selectorsLen;
} GenStream_t;

typedef struct __attribute__((packed)) EventStream {
	GenStream_t streamBase;
	#define st_urgent streamBase.urgent
	#define st_name streamBase.name
	#define st_type streamBase.op_type
	#define st_child streamBase.op_child
	#define st_selectors streamBase.selectors
	#define st_selectorsLen streamBase.selectorsLen
} EventStream_t;

typedef struct __attribute__((packed)) SourceStream {
	GenStream_t streamBase;
	#define st_urgent streamBase.urgent
	#define st_name streamBase.name
	#define st_type streamBase.op_type
	#define st_child streamBase.op_child
	#define st_selectors streamBase.selectors
	#define st_selectorsLen streamBase.selectorsLen
	int period;
	void *timerInfo;
} SourceStream_t;

typedef struct __attribute__((packed)) ObjectStream {
	GenStream_t streamBase;
	#define st_urgent streamBase.urgent
	#define st_name streamBase.name
	#define st_type streamBase.op_type
	#define st_child streamBase.op_child
	#define st_selectors streamBase.selectors
	#define st_selectorsLen streamBase.selectorsLen
	int objectEvents;
} ObjectStream_t;

typedef struct __attribute__((packed)) Predicate {
		unsigned short type;
		Operand_t left;
		Operand_t right;
} Predicate_t;

typedef struct __attribute__((packed)) Filter {
	Operator_t base;
	#define op_type	base.type
	#define op_child	base.child
	unsigned short predicateLen;
	Predicate_t **predicates;
} Filter_t;

typedef struct Select {
	Operator_t base;
	#define op_type	base.type
	#define op_child	base.child
	unsigned short elementsLen;
	Element_t **elements;
} Select_t;

typedef struct __attribute__((packed)) Sort {
	Operator_t base;
	#define op_type	base.type
	#define op_child	base.child
	unsigned short elementsLen;
	Element_t **elements;
	unsigned short sizeUnit;
	unsigned int size;
} Sort_t;
/**
 * NOT IMPLEMENTED
 */

typedef Sort_t Group_t;
/**
 * NOT IMPLEMENTED
 */

typedef struct __attribute__((packed)) Join {
	Operator_t base;
	#define op_type	base.type
	#define op_child	base.child
	Element_t element;
	unsigned short predicateLen;
	Predicate_t **predicates;
} Join_t;
/**
 * Datamodell: Object: +Fkt für 
 * NOT IMPLEMENTED
 */

typedef struct __attribute__((packed)) Aggregate {
	Operator_t base;
	#define op_type	base.type
	#define op_child	base.child
	unsigned short elementsLen;
	Element_t **elements;
	unsigned short sizeUnit;
	unsigned int size;
	unsigned short advanceUnit;
	unsigned int advance;
} Aggregate_t;
/**
 * NOT IMPLEMENTET
 */

typedef struct __attribute__((packed)) Query {
	struct Query *next;								// Since a provider can issue more than one query at a time the next pointer holds the address of the next query. The user has to set it to NULL, if the current instance is the last one.
	Operator_t *root;								// Points to the first element of the actual query, which in fact is of type GEN_{OBJECT,SOURCE,EVENT}.
	unsigned short flags;
	unsigned short idx;
	unsigned int size;
	unsigned int layerCode;
	unsigned int queryID;								// An unique identifier for this query. The first byte is used to address the queries array of a node in the datamodel. The upper bytes contain a global id, which is incremented each time a new query is registered.
	queryCompletedFunction onQueryCompleted;		// A function being called, if a query completes *and* the tupel is not rejected. The called code has to free the tupel!
} Query_t;

static inline void initQuery(Query_t *query) {
	query->next = NULL;
	query->root = NULL;
	query->flags = 0;
	query->idx = 0;
	query->layerCode = LAYER_CODE;
	query->queryID = 0;
	query->onQueryCompleted = NULL;
	query->size = 0;
}

int checkQuerySyntax(DataModelElement_t *rootDM, Operator_t *rootQuery, Operator_t **errOperator, int sync);
int checkQueries(DataModelElement_t *rootDM, Query_t *queries, Operator_t **errOperator, int sync);
#ifdef __KERNEL__
int addQueries(DataModelElement_t *rootDM, Query_t *queries, unsigned long *flags);
#else
int addQueries(DataModelElement_t *rootDM, Query_t *queries);
#endif
#ifdef __KERNEL__
int delQueries(DataModelElement_t *rootDM, Query_t *queries, unsigned long *__flags);
#else
int delQueries(DataModelElement_t *rootDM, Query_t *queries);
#endif
int checkAndSanitizeElementPath(char *elemPath, char **elemPathSani, char **objId);
void printQuery(Operator_t *root);
void freeQuery(Query_t* query);
void executeQuery(DataModelElement_t *rootDM, Query_t *query, Tupel_t *tupel, int step);
int calcQuerySize(Query_t *query);
void copyAndCollectQuery(Query_t *origin, void *freeMem);
void rewriteQueryAddress(Query_t *query, void *oldBaseAddr, void *newBaseAddr);
void freeOperator(Operator_t *op, int freeOperator);
Query_t* resolveQuery(DataModelElement_t *rootDM, QueryID_t *id);

#endif // __QUERY_H__
