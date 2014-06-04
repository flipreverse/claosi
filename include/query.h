#ifndef __QUERY_H__
#define __QUERY_H__

#include <common.h>
#include <datamodel.h>

#define INIT_SRC_STREAM(varName,srcName,isUrgent,childVar,srcFrequency)	strncpy((char*)&varName.st_name,srcName,MAX_NAME_LEN); \
	varName.st_type = GEN_SOURCE; \
	varName.st_child = childVar; \
	varName.st_urgent = isUrgent; \
	varName.frequency = srcFrequency;

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
	varName.predicates = (Predicate_t**)ALLOC(sizeof(Predicate_t) * numPredicates);

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
	OBJECT_CREATE	=	0x0,
	OBJECT_DELETE	=	0x1,
	OBJECT_STATUS	=	0x2
};

enum PredicateType {
	EQUAL			=	0x0,
	LE				=	0x1,
	LEQ				=	0x2,
	GE				=	0x4,
	GEQ				=	0x8,
	IN				=	0x10,
	PREDICATETYPE_END
};

enum OperandType {
	STREAM			=	0x0,
	DATAMODEL		=	0x1,
	POD				=	0x2,
	OPERANDTYPE_END
};

enum SizeUnit {
	EVENTS			=	0x0,
	TIME_MS			=	0x1,
	TIME_SEC		=	0x2,
	SIZEUNIT_END
};

typedef void (*queryCompletedFunction)(void);

typedef struct Operator {
	unsigned short type;
	struct Operator *child;	
} Operator_t;

typedef struct Operand {
	unsigned short type;
	DECLARE_BUFFER(value)
} Operand_t;

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
	int frequency;
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

typedef Sort_t Group_t;

typedef struct Join {
	Operator_t base;
	#define op_type	base.type
	#define op_child	base.child
	Element_t element;
	unsigned short predicateLen;
	Predicate_t **predicates;
} Join_t;

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

typedef struct __attribute__((packed)) Query {
	struct Query *next;
	Operator_t *root;
	unsigned short queryType;
	QueryID_t queryID;
	queryCompletedFunction onQueryCompleted;
} Query_t;

int checkQuerySyntax(DataModelElement_t *rootDM, Operator_t *rootQuery, Operator_t **errOperator);
int checkAndSanitizeElementPath(char *elemPath, char **elemPathSani, char **objId);
void printQuery(Operator_t *root);
void freeQuery(Operator_t *op, int freeOperator);

#endif // __QUERY_H__
