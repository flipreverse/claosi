#include <stdlib.h>
#include <query.h>
#include <datamodel.h>
#include <resultset.h>
#include <stdio.h>
#include <output.h>
#include <time.h>
#include <errno.h>

DECLARE_ELEMENTS(nsNet1, nsProcess, nsUI, model1)
DECLARE_ELEMENTS(evtDisplay, typeEventType, srcForegroundApp, srcProcessess,objApp)
DECLARE_ELEMENTS(typeXPos, typeYPos)
DECLARE_ELEMENTS(objProcess, srcUTime, srcSTime, srcProcessSockets)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen, typeSockRef)
static void initDatamodel(void);

int main() {
	Tupel_t *tupel = NULL, *tupelCompact = NULL, *tupelCompact2 = NULL, *tupleCopy = NULL, *tupleMerge = NULL;
	char *string = NULL, values[] = {66,4,3,2,1};
	clock_t startClock, endClock;
	int size = 0, ret = 0;

	startClock = clock();
	initDatamodel();
	tupel = initTupel(20140530,3);

	allocItem(&model1,tupel,0,"net.device.txBytes");
	setItemInt(&model1,tupel,"net.device.txBytes",4711);
	allocItem(&model1,tupel,1,"net.packetType");
	setItemInt(&model1,tupel,"net.packetType.socket",1337);
	setItemArray(&model1,tupel,"net.packetType.macHdr",5);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",0,1);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",1,2);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",2,3);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",3,4);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",4,5);
	copyArrayByte(&model1,tupel,"net.packetType.macHdr",4,values,5);
	setItemByte(&model1,tupel,"net.packetType.macProtocol",65);
	setItemByte(&model1,tupel,"net.packetType.networkProtocol",42);
	setItemArray(&model1,tupel,"net.packetType.networkHdr",2);
	setArraySlotByte(&model1,tupel,"net.packetType.networkHdr",0,1);
	setArraySlotByte(&model1,tupel,"net.packetType.networkHdr",1,2);
	setItemByte(&model1,tupel,"net.packetType.transportProtocol",21);
	setItemArray(&model1,tupel,"net.packetType.transportHdr",2);
	setArraySlotByte(&model1,tupel,"net.packetType.transportHdr",0,1);
	setArraySlotByte(&model1,tupel,"net.packetType.transportHdr",1,2);
	setItemByte(&model1,tupel,"net.packetType.dataLength",2);

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
	printTupel(&model1,tupel);
	printf("-------------------------\n");

	printf("Deleting an item...\n");
	deleteItem(&model1,tupel,0);
	printTupel(&model1,tupel);
	printf("-------------------------\n");

	printf("Size of tupel: %d\n",getTupelSize(&model1,tupel));

	size = getTupelSize(&model1,tupel);
	if (size != -1) {
		tupelCompact = ALLOC(size);
		if (tupelCompact != NULL) {
			printf("Copy and collecting tuple....");
			ret = copyAndCollectTupel(&model1,tupel,tupelCompact,size);
			freeTupel(&model1,tupel);
			printf("... done. Used %d bytes. Freed the origin tuple.\n", ret);
			printf("-------------------------\n");
			printf("Trying to change rxBytes to LOOOOL\n");
			setItemString(&model1,tupelCompact,"net.device.rxBytes","LOOOOL");
			printTupel(&model1,tupelCompact);
			printf("-------------------------\n");
			printf("Deleting an item...\n");
			deleteItem(&model1,tupelCompact,0);
			printTupel(&model1,tupelCompact);
			printf("-------------------------\n");
			printf("Copying tuple (a.k.a reverting compact)\n");
			tupleCopy = copyTupel(&model1,tupelCompact);
			tupelCompact2 = malloc(size);
			if (tupelCompact2 == NULL) {
				perror("malloc for tupleCompact2");
				return EXIT_FAILURE;
			}
			memcpy(tupelCompact2,tupelCompact,size);
			rewriteTupleAddress(&model1,tupelCompact2,tupelCompact,tupelCompact2);
			freeTupel(&model1,tupelCompact);
			printf("Printing copied tuple:");
			printTupel(&model1,tupleCopy);
			printf("Printing copied and rewritten tuple:");
			printTupel(&model1,tupelCompact2);
			printf("-------------------------\n");
			
			tupleMerge = initTupel(4711,2);
			string = (char*)malloc(6);
			strcpy(string,"PFERD");
			allocItem(&model1,tupleMerge,0,"net.device.rxBytes");
			setItemString(&model1,tupleMerge,"net.device.rxBytes",string);
			allocItem(&model1,tupleMerge,1,"ui.eventType");
			setItemInt(&model1,tupleMerge,"ui.eventType.xPos",314);
			setItemInt(&model1,tupleMerge,"ui.eventType.yPos",42);
			printTupel(&model1,tupleMerge);
			printf("Merging tuple: ");
			printTupel(&model1,tupleMerge);
			mergeTuple(&model1,&tupleCopy,tupleMerge);
			printf("Merged tuple: ");
			printTupel(&model1,tupleCopy);

			freeTupel(&model1,tupleCopy);
			free(tupelCompact2);
		}
	} else {
		freeTupel(&model1,tupel);
	}

	freeDataModel(&model1,0);
	endClock = clock();
	printf("-------------------------\n");
	printf("Start: %ld, end: %ld, diff: %ld/%e\n",startClock, endClock, (endClock - startClock),((double)endClock - (double)startClock) / (double)CLOCKS_PER_SEC);

	return EXIT_SUCCESS;
}


static void regEventCallback(Query_t *query) {
	
}

static void unregEventCallback(Query_t *query) {
	
}

static Tupel_t* getSrc(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	return NULL;
};

static void regObjectCallback(Query_t *query) {
	
};

static void unregObjectCallback(Query_t *query) {
	
};

static Tupel_t* generateStatusObject(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	return NULL;
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
	INIT_PLAINTYPE(typeTransProt,"transportProtocol",typePacketType,BYTE)
	INIT_PLAINTYPE(typeDataLen,"dataLength",typePacketType,BYTE)
	INIT_REF(typeSockRef,"socket",typePacketType,"process.process.sockets")

	INIT_COMPLEX_TYPE(typePacketType,"packetType",nsNet1,8)
	ADD_CHILD(typePacketType,3,typeMacHdr);
	ADD_CHILD(typePacketType,0,typeMacProt);
	ADD_CHILD(typePacketType,1,typeNetProt);
	ADD_CHILD(typePacketType,2,typeNetHdr);
	ADD_CHILD(typePacketType,4,typeTranspHdr);
	ADD_CHILD(typePacketType,7,typeTransProt);
	ADD_CHILD(typePacketType,6,typeDataLen);
	ADD_CHILD(typePacketType,5,typeSockRef);

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
	INIT_COMPLEX_TYPE(typeEventType,"eventType",nsUI,2)
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
