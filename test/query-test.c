#include <stdlib.h>
#include <query.h>
#include <datamodel.h>
#include <resultset.h>
#include <stdio.h>
#include <debug.h>

DECLARE_ELEMENTS(nsNet1, nsProcess, nsUI, model1)
DECLARE_ELEMENTS(evtDisplay, typeEventType, srcForegroundApp, srcProcessess,objApp)
DECLARE_ELEMENTS(typeXPos, typeYPos)
DECLARE_ELEMENTS(objProcess, srcUTime, srcSTime, srcProcessSockets)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen, typeSockRef)
static void initDatamodel(void);
static void initResultset(void);

SourceStream_t txSrc;
ObjectStream_t processObj;
EventStream_t txStream, rxStream;
Join_t joinProcess, joinApp;
Predicate_t joinProcessPredicate, joinAppPredicate, filterTXPredicate,filterRXPredicate;
Filter_t filter;
Select_t selectTest;
Tupel_t *tupel = NULL;
Query_t query;
Element_t elemPacket, elemUTime;

void printResult(QueryID_t id, Tupel_t *tupel) {
	printf("Received tupel:\t");
	printTupel(&model1,tupel);
}

int main() {
	int ret = 0;
	Operator_t *errOperator = NULL;

	query.next = NULL;
	query.queryType = SYNC;
	query.queryID = 0;
	query.onQueryCompleted = printResult;

	initDatamodel();
	initResultset();
	
	INIT_EVT_STREAM(txStream,"net.device.onTx",0,GET_BASE(filter))
	INIT_FILTER(filter,GET_BASE(selectTest),2)
	ADD_PREDICATE(filter,0,filterTXPredicate)
	ADD_PREDICATE(filter,1,filterRXPredicate)
	SET_PREDICATE(filterTXPredicate,EQUAL, STREAM, "net.packetType.macProtocol", POD, "65")
	SET_PREDICATE(filterRXPredicate,GEQ, STREAM, "process.process.utime", POD, "3.14")
	INIT_SELECT(selectTest,NULL,2)
	ADD_ELEMENT(selectTest,0,elemUTime,"process.process.utime")
	ADD_ELEMENT(selectTest,1,elemPacket,"net.packetType")
	if ((ret = checkQuerySyntax(&model1,GET_BASE(txStream),&errOperator)) == 0) {
		printQuery(GET_BASE(txStream));
	} else {
		printf("Failed. Reason: %d\n",-ret);
	}
	query.root = GET_BASE(txStream);
	executeQuery(&model1,&query,&tupel);

	INIT_SRC_STREAM(txSrc,"process.process.utime",0,GET_BASE(joinProcess),100)
	INIT_JOIN(joinProcess,"process.process", GET_BASE(joinApp),1)
	ADD_PREDICATE(joinProcess,0,joinProcessPredicate)
	SET_PREDICATE(joinProcessPredicate,IN, STREAM, "net.packetType.socket", STREAM, "process.process.sockets")
	INIT_JOIN(joinApp,"ui.app", NULL,1)
	ADD_PREDICATE(joinApp,0,joinAppPredicate)
	SET_PREDICATE(joinAppPredicate,IN, STREAM, "process.process", STREAM, "ui.app.processes")
	if ((ret = checkQuerySyntax(&model1,GET_BASE(txSrc),&errOperator)) == 0) {
		printQuery(GET_BASE(txSrc));
	} else {
		printf("Failed. Reason: %d\n",-ret);
	}
	
	INIT_OBJ_STREAM(processObj,"process.process",0,NULL,OBJECT_CREATE)
	if ((ret = checkQuerySyntax(&model1,GET_BASE(processObj),&errOperator)) == 0) {
		printQuery(GET_BASE(processObj));
	} else {
		printf("Failed. Reason: %d\n",-ret);
	}
	
	freeQuery(GET_BASE(processObj),0);
	freeQuery(GET_BASE(txStream),0);
	freeQuery(GET_BASE(txSrc),0);
	if (tupel != NULL) {
		freeTupel(&model1,tupel);
	}
	freeDataModel(&model1,0);

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

static Tupel_t* generateStatusObject(void) {
	return NULL;
}

static void initResultset(void) {
	char *string = NULL;

	tupel = initTupel(20140530,3);

	allocItem(&model1,tupel,0,"net.device.txBytes");
	setItemInt(&model1,tupel,"net.device.txBytes",4711);

	allocItem(&model1,tupel,1,"net.packetType");
	setItemArray(&model1,tupel,"net.packetType.macHdr",4);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",0,1);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",1,2);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",2,3);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",3,4);
	setItemByte(&model1,tupel,"net.packetType.macProtocol",65);

	string = (char*)malloc(6);
	strcpy(string,"PFERD");
	allocItem(&model1,tupel,2,"net.device.rxBytes");
	setItemString(&model1,tupel,"net.device.rxBytes",string);
	
	string = (char*)malloc(5);
	strcpy(string,"eth0");
	addItem(&tupel,3);
	allocItem(&model1,tupel,3,"net.device");
	setItemString(&model1,tupel,"net.device",string);

	allocItem(&model1,tupel,4,"process.process.utime");
	setItemFloat(&model1,tupel,"process.process.utime",3.14);

	allocItem(&model1,tupel,5,"process.process.stime");
	setItemArray(&model1,tupel,"process.process.stime",3);
	string = (char*)malloc(3);
	strcpy(string,"Ö");
	setArraySlotString(&model1,tupel,"process.process.stime",0,string);
	string = (char*)malloc(3);
	strcpy(string,"Ä");
	setArraySlotString(&model1,tupel,"process.process.stime",1,string);
	string = (char*)malloc(3);
	strcpy(string,"Ü");
	setArraySlotString(&model1,tupel,"process.process.stime",2,string);
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
	INIT_REF(typeSockRef,"socket",typePacketType,"process.process.sockets")

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
