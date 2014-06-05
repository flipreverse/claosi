#include <stdlib.h>
#include <query.h>
#include <string.h>
#include <stdio.h>

DECLARE_ELEMENTS(nsNet1, nsProcess, nsUI, model1)
DECLARE_ELEMENTS(evtDisplay, typeEventType, srcForegroundApp, srcProcessess,objApp)
DECLARE_ELEMENTS(typeXPos, typeYPos)
DECLARE_ELEMENTS(objProcess, srcUTime, srcSTime, srcProcessSockets)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen, typeSockRef)
DECLARE_ELEMENTS(model2, srcDelayTolerance, typePacketType2, nsNet2, objDevice2, srcState)

void regEventCallback(eventOccured pCallback) {
	
}

void unregEventCallback(eventOccured pCallback) {
	
}

void* getSrc(void) {
	return NULL;
};

void regObjectCallback(objectChanged pCallback) {
	
};

void unregObjectCallback(objectChanged pCallback) {
	
};

int main() {
	int ret = 0;
	DataModelElement_t *errNode = NULL, *copy = NULL;

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

	INIT_TYPE(typePacketType,"packetType",nsNet1,8)
	ADD_CHILD(typePacketType,0,typeMacHdr);
	ADD_CHILD(typePacketType,1,typeMacProt);
	ADD_CHILD(typePacketType,2,typeNetHdr);
	ADD_CHILD(typePacketType,3,typeNetProt);
	ADD_CHILD(typePacketType,4,typeTranspHdr);
	ADD_CHILD(typePacketType,5,typeTransProt);
	ADD_CHILD(typePacketType,6,typeDataLen);
	ADD_CHILD(typePacketType,7,typeSockRef);

	INIT_SOURCE_POD(srcTXBytes,"txBytes",objDevice,INT,getSrc)
	INIT_SOURCE_POD(srcRXBytes,"rxBytes",objDevice,INT,getSrc)
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

	INIT_SOURCE_POD(srcDelayTolerance,"delayTolerance",nsNet2,INT,getSrc)
	INIT_SOURCE_POD(srcState,"state",objDevice2,INT,getSrc)
	INIT_OBJECT(objDevice2,"device",nsNet2,1,STRING,regObjectCallback,unregObjectCallback)
	ADD_CHILD(objDevice2,0,srcState)
	
	INIT_TYPE(typePacketType2,"packetType",nsNet2,0)
	
	INIT_NS(nsNet2,"net",model2,2)
	ADD_CHILD(nsNet2,0,srcDelayTolerance)
	ADD_CHILD(nsNet2,1,objDevice2)

	INIT_MODEL(model2,1)
	ADD_CHILD(model2,0,nsNet2)
	
	//printDatamodel(&model1);
	//printDatamodel(&model2);
	printf("-------------------------\n");
	copy = copySubtree(&model1);
	printDatamodel(copy);
	printf("-------------------------\n");

	printf("Is syntax correct? ...");
	if ((ret = checkSyntax(NULL,copy,&errNode)) == 0) {
		printf("yes\nIs datamodel mergeable? ...");
		if ((ret = mergeDataModel(1,copy,&model2)) != 0) {
			printf("not mergable; errcode = 0x%x\n",-ret);
		} else {
			printf("yes\nMerging it...");
			mergeDataModel(0,copy,&model2);
			printf(" Done.\n");
			printDatamodel(copy);
		}
	} else {
		printf("errcode = 0x%x\n",-ret);
		printDatamodel(errNode);
	}
	printf("-------------------------\n");
	deleteSubtree(&copy,&model2);
	if (copy != NULL) {
		printDatamodel(copy);
	} else {
		printf("Datamodel was completely erased.\n");
	}
	printf("-------------------------\n");
	errNode = getDescription(copy,"net.packetType");
	printf("offset of transportHdr: %d\n",getOffset(errNode,"transportHdr"));
	printf("offset of macHdr: %d\n",getOffset(errNode,"macHdr"));
	printf("-------------------------\n");
	printf(".... found at %p\n",getDescription(copy,"foo"));
	printf(".... found at %p\n",getDescription(copy,"net"));
	printf(".... found at %p\n",getDescription(copy,"ui.eventType.xPos"));
	printf(".... found at %p\n",getDescription(copy,"net.packetType.macProtocol"));
	printf(".... found at %p\n",getDescription(copy,""));
	printf(".... found at %p\n",getDescription(copy,"net.device.rxBytes"));
	printf("-------------------------\n");

	freeSubtree(&model1,0);
	freeSubtree(&model2,0);
	freeSubtree(copy,1);

	return EXIT_SUCCESS;
}
