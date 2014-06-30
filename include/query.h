#ifndef __QUERY_H__
#define __QUERY_H__

#include <common.h>
#include <datamodel.h>
#include <resultset.h>

/**
 * Generates an unique query which will be assigned to Query_t.queryID
 */
#define MAKE_QUERY_ID(globalID,localID)	((globalID << 8) | (localID & 0xf))
#define GET_LOCAL_QUERY_ID(queryID)		(queryID & 0xf)
#define GET_GLOBAL_QUERY_ID(queryID)	(queryID >> 8)

#define GET_BASE(varName)	(Operator_t*)&varName
#define ADD_PREDICATE(varOperator,slot,predicateVar)	varOperator.predicates[slot] = &predicateVar;
#define ADD_ELEMENT(varOperator,slot,elementVar,elementName)	varOperator.elements[slot] = &elementVar; \
	strncpy((char*)&elementVar.name,elementName,MAX_NAME_LEN);

#define INIT_SRC_STREAM(varName,srcName,isUrgent,childVar,srcPeriod)	strncpy((char*)&varName.st_name,srcName,MAX_NAME_LEN); \
	varName.st_type = GEN_SOURCE; \
	varName.st_child = childVar; \
	varName.st_urgent = isUrgent; \
	varName.period = srcPeriod; \
	varName.timerInfo = NULL;

#define INIT_OBJ_STREAM(varName,objName,isUrgent,childVar,listObjEvents)	strncpy((char*)&varName.st_name,objName,MAX_NAME_LEN); \
	varName.st_type = GEN_OBJECT; \
	varName.st_child = childVar; \
	varName.st_urgent = isUrgent; \
	varName.objectEvents = listObjEvents;

#define INIT_EVT_STREAM(varName,evtName,isUrgent,childVar)	strncpy((char*)&varName.st_name,evtName,MAX_NAME_LEN); \
	varName.st_type = GEN_EVENT; \
	varName.st_child = childVar; \
	varName.st_urgent = isUrgent;

#define INIT_JOIN(varName,joinElement,childVar,numPredicates)	strncpy((char*)&varName.element.name,joinElement,MAX_NAME_LEN); \
	varName.op_type = JOIN; \
	varName.op_child = childVar; \
	varName.predicateLen = numPredicates; \
	varName.predicates = (Predicate_t**)ALLOC(sizeof(Predicate_t**) * numPredicates);

#define INIT_FILTER(varName,childVar,numPredicates)	varName.op_type = FILTER; \
	varName.op_child = childVar; \
	varName.predicateLen = numPredicates; \
	varName.predicates = (Predicate_t**)ALLOC(sizeof(Predicate_t**) * numPredicates);

#define INIT_SELECT(varName,childVar,numElements) varName.op_type = SELECT; \
	varName.op_child = childVar; \
	varName.elementsLen = numElements; \
	varName.elements = (Element_t**)ALLOC(sizeof(Element_t**) * numElements);

#define SET_PREDICATE(varName, predType, predLeftType, predLeftName, predRightType, predRightName)	varName.type = predType; \
	varName.left.type = predLeftType; \
	strncpy((char*)&varName.left.value,predLeftName,MAX_NAME_LEN); \
	varName.right.type = predRightType; \
	strncpy((char*)&varName.right.value,predRightName,MAX_NAME_LEN);

enum QueryType {
	SYNC		=	0,
	ASYNC		=	1
};

typedef unsigned short	QueryID_t;

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
	IN,
	PREDICATETYPE_END
};

enum OperandType {
	STREAM			=	0x0,
	POD,
	OPERANDTYPE_END
};

enum SizeUnit {
	EVENTS			=	0x0,
	TIME_MS			=	0x1,
	TIME_SEC		=	0x2,
	SIZEUNIT_END
};

typedef void (*queryCompletedFunction)(QueryID_t,Tupel_t*);

/**
 * Baseclass for a query. Each element of a query uses this struct.
 */
typedef struct Operator {
	unsigned short type;
	struct Operator *child;	
} Operator_t;
/**
 * Used to describe the left-hand and right-hand side of a predicate
 */
typedef struct Operand {
	unsigned short type;
	DECLARE_BUFFER(value)
} Operand_t;
/**
 * Describes a single element of the datamodel.
 * For now, it just contains a string holding the complete path to the desired element.
 */
typedef struct Element {
	DECLARE_BUFFER(name)
} Element_t;

typedef struct GenStream {
	Operator_t base;
	#define op_type	base.type
	#define op_child	base.child
	char urgent;
	DECLARE_BUFFER(name)
} GenStream_t;

typedef struct EventStream {
	GenStream_t streamBase;
	#define st_urgent streamBase.urgent
	#define st_name streamBase.name
	#define st_type streamBase.op_type
	#define st_child streamBase.op_child
} EventStream_t;

typedef struct SourceStream {
	GenStream_t streamBase;
	#define st_urgent streamBase.urgent
	#define st_name streamBase.name
	int period;
	void *timerInfo;
} SourceStream_t;

typedef struct ObjectStream {
	GenStream_t streamBase;
	#define st_urgent streamBase.urgent
	#define st_name streamBase.name
	int objectEvents;
} ObjectStream_t;

typedef struct Predicate {
		unsigned short type;
		Operand_t left;
		Operand_t right;
} Predicate_t;

typedef struct Filter {
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

typedef struct Sort {
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

typedef struct Join {
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

typedef struct Aggregate {
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
	unsigned short queryType;						// Indicates an synchronous or synchronoues query. The letter one will be executed immediately. Therefore it can only be a source or object-status query. TODO: This member may be obsolete.
	QueryID_t queryID;								// An unique identifier for this query. The first byte is used to address the queries array of a node in the datamodel. The upper bytes contain a global id, which is incremented each time a new query is registered.
	queryCompletedFunction onQueryCompleted;		// A function being called, if a query completes *and* the tupel is not rejected. The called code has to free the tupel!
} Query_t;

int checkQuerySyntax(DataModelElement_t *rootDM, Operator_t *rootQuery, Operator_t **errOperator);
int checkQueries(DataModelElement_t *rootDM, Query_t *queries, Operator_t **errOperator, int syncAllowed);
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
void freeQuery(Operator_t *op, int freeOperator);
void executeQuery(DataModelElement_t *rootDM, Query_t *query, Tupel_t **tupel);

#endif // __QUERY_H__
