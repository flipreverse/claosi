#include <stdlib.h>
#include <query.h>
#include <datamodel.h>
#include <resultset.h>
#include <stdio.h>
#include <output.h>
#include <api.h>
#include <time.h>
#include <errno.h>
#include <communication.h>

DECLARE_ELEMENTS(nsNet1, nsProcess, nsUI, model1)
DECLARE_ELEMENTS(evtDisplay, typeEventType, srcForegroundApp, srcProcessess,objApp)
DECLARE_ELEMENTS(typeXPos, typeYPos)
DECLARE_ELEMENTS(objProcess, srcUTime, srcSTime, srcProcessSockets)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen)
static void initDatamodel(void);
static void setupQueries(void);
static void issueEvent(void);

static ObjectStream_t processObjStream;
static Join_t joinProcessStime;
static Predicate_t joinProcessStimePredicate, joinProcessOP_PODPredicate;
static Query_t query;
static Tupel_t *tupel = NULL;
static unsigned int foo = 1;

void printResult(unsigned int id, Tupel_t *tuple) {
	Tupel_t *tempTuple = NULL;

	while (tuple != NULL) {
		printf("Received tupel:\t");
		printTupel(&model1,tuple);
		tempTuple = tuple->next;
		freeTupel(&model1,tuple);
		tuple = tempTuple;
	}
}

int main() {
	int ret = 0;
	clock_t startClock, endClock;

	startClock = clock();

	initDatamodel();
	setupQueries();
	globalQueryID = &foo;

	if (initSLC() == -1) {
		return EXIT_FAILURE;
	}
	INIT_MODEL((*SLC_DATA_MODEL),0);
	if ((ret = registerProvider(&model1, &query)) < 0 ) {
		printf("Register failed: %d\n",-ret);
		return EXIT_FAILURE;
	}
	printf("Sucessfully registered datamodel and query. Query has id: 0x%x\n",query.queryID);

	issueEvent();

	if ((ret = unregisterProvider(&model1, &query)) < 0 ) {
		printf("Unregister failed: %d\n",-ret);
		return EXIT_FAILURE;
	}

	freeOperator(GET_BASE(processObjStream),0);
	freeDataModel(&model1,0);
	destroySLC();
	endClock = clock();
	printf("Start: %ld, end: %ld, diff: %ld/%e\n",startClock, endClock, (endClock - startClock),((double)endClock - (double)startClock) / (double)CLOCKS_PER_SEC);

	return EXIT_SUCCESS;
}


static void regEventCallback(Query_t *query) {
	
}

static void unregEventCallback(Query_t *query) {
	
}

static Tupel_t* getSrc(Selector_t *selectors, int len) {
	Tupel_t *tuple = NULL;

	tuple = initTupel(20140531,2);
	allocItem(SLC_DATA_MODEL,tuple,0,"process.process");
	setItemInt(SLC_DATA_MODEL,tuple,"process.process",4711);
	allocItem(SLC_DATA_MODEL,tuple,1,"process.process.stime");
	setItemInt(SLC_DATA_MODEL,tuple,"process.process.stime",42);
	
	tuple->next = initTupel(20140712,2);
	allocItem(SLC_DATA_MODEL,tuple->next,0,"process.process");
	setItemInt(SLC_DATA_MODEL,tuple->next,"process.process",1);
	allocItem(SLC_DATA_MODEL,tuple->next,1,"process.process.stime");
	setItemInt(SLC_DATA_MODEL,tuple->next,"process.process.stime",21);

	return tuple;
};

static void regObjectCallback(Query_t *query) {
	
};

static void unregObjectCallback(Query_t *query) {
	
};

static Tupel_t* generateStatusObject(Selector_t *selectors, int len) {
	return NULL;
}

static void issueEvent(void) {
	tupel = initTupel(20140530,1);

	allocItem(SLC_DATA_MODEL,tupel,0,"process.process");
	setItemInt(SLC_DATA_MODEL,tupel,"process.process",1);

	objectChangedBroadcast("process.process",tupel,OBJECT_CREATE);
}

static void setupQueries(void) {
	initQuery(&query);
	query.onQueryCompleted = printResult;
	query.root = GET_BASE(processObjStream);

	INIT_OBJ_STREAM(processObjStream,"process.process",0,0,GET_BASE(joinProcessStime),OBJECT_CREATE)
	INIT_JOIN(joinProcessStime,"process.process.stime", NULL,2)
	ADD_PREDICATE(joinProcessStime,0,joinProcessOP_PODPredicate)
	SET_PREDICATE(joinProcessOP_PODPredicate,EQUAL, OP_POD, "1", OP_POD, "1")
	ADD_PREDICATE(joinProcessStime,1,joinProcessStimePredicate)
	SET_PREDICATE(joinProcessStimePredicate,EQUAL, OP_JOIN, "process.process", OP_STREAM, "process.process")
	
}

static void initDatamodel(void) {
	int i = 0;
	INIT_SOURCE_POD(srcSocketType,"type",objSocket,INT,getSrc)
	INIT_SOURCE_POD(srcSocketFlags,"flags",objSocket,INT,getSrc)
	INIT_OBJECT(objSocket,"socket",nsNet1,2,INT,regObjectCallback,unregObjectCallback,generateStatusObject)
	ADD_CHILD(objSocket,0,srcSocketFlags)
	ADD_CHILD(objSocket,1,srcSocketType)
	
	INIT_PLAINTYPE(typeMacHdr,"macHdr",typePacketType,(BYTE | ARRAY))
	INIT_PLAINTYPE(typeMacProt,"macProtocol",typePacketType,BYTE)
	INIT_PLAINTYPE(typeNetHdr,"networkHdr",typePacketType,(BYTE | ARRAY))
	INIT_PLAINTYPE(typeNetProt,"networkProtocol",typePacketType,BYTE)
	INIT_PLAINTYPE(typeTranspHdr,"transportHdr",typePacketType,(BYTE | ARRAY))
	INIT_PLAINTYPE(typeTransProt,"transportProtocl",typePacketType,BYTE)
	INIT_PLAINTYPE(typeDataLen,"dataLength",typePacketType,BYTE)
	//INIT_REF(typeSockRef,"socket",typePacketType,"process.process.sockets")

	INIT_TYPE(typePacketType,"packetType",nsNet1,2)
	ADD_CHILD(typePacketType,0,typeMacHdr);
	ADD_CHILD(typePacketType,1,typeMacProt);
	/*ADD_CHILD(typePacketType,2,typeNetHdr);
	ADD_CHILD(typePacketType,3,typeNetProt);
	ADD_CHILD(typePacketType,4,typeTranspHdr);
	ADD_CHILD(typePacketType,5,typeTransProt);
	ADD_CHILD(typePacketType,6,typeDataLen);
	ADD_CHILD(typePacketType,7,typeSockRef);*/

	INIT_SOURCE_POD(srcTXBytes,"txBytes",objDevice,INT,getSrc)
	INIT_SOURCE_POD(srcRXBytes,"rxBytes",objDevice,STRING,getSrc)
	INIT_EVENT_COMPLEX(evtOnRX,"onRx",objDevice,"net.packetType",regEventCallback,unregEventCallback)
	INIT_EVENT_COMPLEX(evtOnTX,"onTx",objDevice,"net.packetType",regEventCallback,unregEventCallback)

	INIT_OBJECT(objDevice,"device",nsNet1,4,STRING,regObjectCallback,unregObjectCallback,generateStatusObject)
	ADD_CHILD(objDevice,0,srcTXBytes)
	ADD_CHILD(objDevice,1,srcRXBytes)
	ADD_CHILD(objDevice,2,evtOnRX)
	ADD_CHILD(objDevice,3,evtOnTX)
	
	INIT_NS(nsNet1,"net",model1,3)
	ADD_CHILD(nsNet1,0,objDevice)
	ADD_CHILD(nsNet1,1,objSocket)
	ADD_CHILD(nsNet1,2,typePacketType)

	INIT_SOURCE_POD(srcUTime,"utime",objProcess,FLOAT,getSrc)
	INIT_SOURCE_POD(srcSTime,"stime",objProcess,STRING|ARRAY,getSrc)
	pthread_rwlock_init(&((Source_t*)srcSTime.typeInfo)->lock,NULL);
	INIT_SOURCE_COMPLEX(srcProcessSockets,"sockets",objProcess,"net.socket",getSrc) //TODO: Should be an array
	INIT_OBJECT(objProcess,"process",nsProcess,3,INT,regObjectCallback,unregObjectCallback,generateStatusObject)
	ADD_CHILD(objProcess,0,srcUTime)
	ADD_CHILD(objProcess,1,srcSTime)
	ADD_CHILD(objProcess,2,srcProcessSockets)

	INIT_NS(nsProcess,"process",model1,1)
	ADD_CHILD(nsProcess,0,objProcess)

	INIT_EVENT_COMPLEX(evtDisplay,"display",nsUI,"ui.eventType",regEventCallback,unregEventCallback)
	INIT_SOURCE_COMPLEX(srcProcessess,"processes",objApp,"process.process",getSrc) //TODO: should be an array as well
	
	INIT_OBJECT(objApp,"app",nsUI,1,STRING,regObjectCallback,unregObjectCallback,generateStatusObject)
	ADD_CHILD(objApp,0,srcProcessess)

	INIT_SOURCE_COMPLEX(srcForegroundApp,"foregroundApp",nsUI,"ui.app",getSrc)
	
	INIT_PLAINTYPE(typeXPos,"xPos",typeEventType,INT)
	INIT_PLAINTYPE(typeYPos,"yPos",typeEventType,INT)
	INIT_TYPE(typeEventType,"eventType",nsUI,2)
	ADD_CHILD(typeEventType,0,typeXPos)
	ADD_CHILD(typeEventType,1,typeYPos)

	INIT_NS(nsUI,"ui",model1,4)
	ADD_CHILD(nsUI,0,evtDisplay)
	ADD_CHILD(nsUI,1,typeEventType)
	ADD_CHILD(nsUI,2,srcForegroundApp)
	ADD_CHILD(nsUI,3,objApp)
	
	INIT_MODEL(model1,3)
	ADD_CHILD(model1,0,nsNet1)
	ADD_CHILD(model1,1,nsProcess)
	ADD_CHILD(model1,2,nsUI)
}
