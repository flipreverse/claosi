#include <stdlib.h>
#include <query.h>
#include <datamodel.h>
#include <resultset.h>
#include <stdio.h>
#include <debug.h>
#include <api.h>
#include <time.h>

DECLARE_ELEMENTS(nsNet1, nsProcess, nsUI, model1)
DECLARE_ELEMENTS(evtDisplay, typeEventType, srcForegroundApp, srcProcessess,objApp)
DECLARE_ELEMENTS(typeXPos, typeYPos)
DECLARE_ELEMENTS(objProcess, srcUTime, srcSTime, srcProcessSockets)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen)
static void initDatamodel(void);
static void initQuery(void);
static void issueEvent(void);

static EventStream_t txStream;
static Predicate_t filterTXPredicate;
static Filter_t filter;
static Select_t selectTest;
static Query_t query;
static Element_t elemPacket;
static Tupel_t *tupel = NULL;

void printResult(QueryID_t id, Tupel_t *tupel) {
	printf("Received tupel:\t");
	printTupel(&model1,tupel);
	freeTupel(&model1,tupel);
}

int main() {
	int ret = 0;
	clock_t startClock, endClock;

	startClock = clock();
	initDatamodel();
	initQuery();

	if (initSLC() == -1) {
		return EXIT_FAILURE;
	}
	INIT_MODEL((*slcDataModel),0);
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

	freeQuery(GET_BASE(txStream),0);
	freeDataModel(&model1,0);
	destroySLC();
	endClock = clock();
	printf("Start: %ld, end: %ld, diff: %ld/%e\n",startClock, endClock, (endClock - startClock),((double)endClock - (double)startClock) / (double)CLOCKS_PER_SEC);

	return EXIT_SUCCESS;
}


static void regEventCallback(void) {
	
}

static void unregEventCallback(void) {
	
}

static void* getSrc(void) {
	return NULL;
};

static void regObjectCallback(void) {
	
};

static void unregObjectCallback(void) {
	
};

static void issueEvent(void) {
	initTupel(&tupel,20140530,2);

	allocItem(slcDataModel,tupel,0,"net.device.txBytes");
	setItemInt(slcDataModel,tupel,"net.device.txBytes",4711);

	allocItem(slcDataModel,tupel,1,"net.packetType");
	setItemArray(slcDataModel,tupel,"net.packetType.macHdr",4);
	setArraySlotByte(slcDataModel,tupel,"net.packetType.macHdr",0,1);
	setArraySlotByte(slcDataModel,tupel,"net.packetType.macHdr",1,2);
	setArraySlotByte(slcDataModel,tupel,"net.packetType.macHdr",2,3);
	setArraySlotByte(slcDataModel,tupel,"net.packetType.macHdr",3,4);
	setItemByte(slcDataModel,tupel,"net.packetType.macProtocol",65);
	
	eventOccured("net.device.onTx",tupel);
}

static void initQuery(void) {
	query.next = NULL;
	query.queryType = ASYNC;
	query.queryID = 0;
	query.onQueryCompleted = printResult;
	query.root = GET_BASE(txStream);

	INIT_EVT_STREAM(txStream,"net.device.onTx",0,GET_BASE(filter))
	INIT_FILTER(filter,GET_BASE(selectTest),1)
	ADD_PREDICATE(filter,0,filterTXPredicate)
	SET_PREDICATE(filterTXPredicate,EQUAL, STREAM, "net.packetType.macProtocol", POD, "65")
	INIT_SELECT(selectTest,NULL,1)
	ADD_ELEMENT(selectTest,0,elemPacket,"net.packetType")
}

static void initDatamodel(void) {
	int i = 0;
	INIT_SOURCE_POD(srcSocketType,"type",objSocket,INT,getSrc)
	INIT_SOURCE_POD(srcSocketFlags,"flags",objSocket,INT,getSrc)
	INIT_OBJECT(objSocket,"socket",nsNet1,2,INT,regObjectCallback,unregObjectCallback)
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

	INIT_OBJECT(objDevice,"device",nsNet1,4,STRING,regObjectCallback,unregObjectCallback)
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
	INIT_SOURCE_COMPLEX(srcProcessSockets,"sockets",objProcess,"net.socket",getSrc) //TODO: Should be an array
	INIT_OBJECT(objProcess,"process",nsProcess,3,INT,regObjectCallback,unregObjectCallback)
	ADD_CHILD(objProcess,0,srcUTime)
	ADD_CHILD(objProcess,1,srcSTime)
	ADD_CHILD(objProcess,2,srcProcessSockets)

	INIT_NS(nsProcess,"process",model1,1)
	ADD_CHILD(nsProcess,0,objProcess)

	INIT_EVENT_COMPLEX(evtDisplay,"display",nsUI,"ui.eventType",regEventCallback,unregEventCallback)
	INIT_SOURCE_COMPLEX(srcProcessess,"processes",objApp,"process.process",getSrc) //TODO: should be an array as well
	
	INIT_OBJECT(objApp,"app",nsUI,1,STRING,regObjectCallback,unregObjectCallback)
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
