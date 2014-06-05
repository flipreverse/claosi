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

int main() {
	Tupel_t *tupel = NULL, *tupelCompact = NULL;

	initDatamodel();

	initTupel(&tupel,20140530,3);

	allocItem(&model1,tupel,0,"net.device.txBytes");
	setItemInt(&model1,tupel,"net.device.txBytes",4711);

	allocItem(&model1,tupel,1,"net.packetType");
	setItemArray(&model1,tupel,"net.packetType.macHdr",4);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",0,1);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",1,2);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",2,3);
	setArraySlotByte(&model1,tupel,"net.packetType.macHdr",3,4);
	setItemByte(&model1,tupel,"net.packetType.macProtocol",65);

	allocItem(&model1,tupel,2,"net.device.rxBytes");
	setItemString(&model1,tupel,"net.device.rxBytes","PFERD");

	printf("test=%s\n",getItemString(&model1,tupel,"net.device.rxBytes"));
	printf("test=%d\n",getItemInt(&model1,tupel,"net.device.txBytes"));
	printf("test=%hhd\n",getArraySlotByte(&model1,tupel,"net.packetType.macHdr",3));

	printTupel(&model1,tupel);
	printf("Size of tupel: %d\n",getTupelSize(&model1,tupel));
	tupelCompact = copyAndCollectTupel(&model1,tupel);
	freeTupel(&model1,tupel);
	setItemString(&model1,tupelCompact,"net.device.rxBytes","LOOOOL");
	printTupel(&model1,tupelCompact);
	freeTupel(&model1,tupelCompact);
	freeSubtree(&model1,0);

	return EXIT_SUCCESS;
}


static void regEventCallback(eventOccured pCallback) {
	
}

static void unregEventCallback(eventOccured pCallback) {
	
}

static void* getSrc(void) {
	return NULL;
};

static void regObjectCallback(objectChanged pCallback) {
	
};

static void unregObjectCallback(objectChanged pCallback) {
	
};

static void initDatamodel(void) {
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

	INIT_OBJECT(objDevice,"device",nsNet1,4,STRING,regObjectCallback,unregObjectCallback)
	ADD_CHILD(objDevice,0,srcTXBytes)
	ADD_CHILD(objDevice,1,srcRXBytes)
	ADD_CHILD(objDevice,2,evtOnRX)
	ADD_CHILD(objDevice,3,evtOnTX)
	
	INIT_NS(nsNet1,"net",model1,3)
	ADD_CHILD(nsNet1,0,objDevice)
	ADD_CHILD(nsNet1,1,objSocket)
	ADD_CHILD(nsNet1,2,typePacketType)

	INIT_SOURCE_POD(srcUTime,"utime",objProcess,INT,getSrc)
	INIT_SOURCE_POD(srcSTime,"stime",objProcess,INT,getSrc)
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
