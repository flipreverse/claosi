#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

DECLARE_ELEMENTS(nsNet, model)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen)
static void initDatamodel(void);
static void initQuery(void);
static EventStream_t txStream;
static Predicate_t filterTXPredicate;
static Filter_t filter;
static Select_t selectTest;
static Query_t query;
static Element_t elemPacket;
static int handler_pre(struct kprobe *p, struct pt_regs *regs);

static struct kprobe rxKP = {
	.symbol_name	= "__netif_receive_skb",
	.pre_handler = handler_pre
};

static int handler_pre(struct kprobe *p, struct pt_regs *regs) {
	struct sk_buff *skb = NULL;
	Tupel_t *tupel = NULL;
	struct timeval time;

#if defined(__i386__)
skb = (struct sk_buff*)regs->ax;
#elif defined(__x86_64__)
skb = (struct sk_buff*)regs->ax;
#elif defined(__arm__)
skb = (struct sk_buff*)regs->ARM_r0;
#else
#error Unknown architecture
#endif

	do_gettimeofday(&time);
	if ((tupel = initTupel(time.tv_sec + time.tv_usec / USEC_PER_MSEC,2)) == NULL) {
		return 0;
	}

	allocItem(slcDataModel,tupel,0,"net.device.txBytes");
	setItemInt(slcDataModel,tupel,"net.device.txBytes",4711);
	allocItem(slcDataModel,tupel,1,"net.packetType");
	setItemArray(slcDataModel,tupel,"net.packetType.macHdr",ETH_HLEN);
	copyArrayByte(slcDataModel,tupel,"net.packetType.macHdr",0,skb->data,ETH_HLEN);
	setItemByte(slcDataModel,tupel,"net.packetType.macProtocol",42);
	PRINT_MSG("Received paket at %lu us\n",time.tv_sec * USEC_PER_MSEC + time.tv_usec);
	eventOccured("net.device.onTx",tupel);

	return 0;
}

static void activate(void) {
	int ret = 0;
	
	if ((ret = register_kprobe(&rxKP)) < 0) {
		DEBUG_MSG(1,",register_kprobe failed. Reason: %d\n",ret);
		return;
	}
	DEBUG_MSG(1,"Registered kprobe at %s\n",rxKP.symbol_name);
}

static void deactivate(void) {
	unregister_kprobe(&rxKP);
	DEBUG_MSG(1,"Unregistered kprobes at %s\n",rxKP.symbol_name);
}

static void* getSrc(void) {
	return NULL;
};

static void printResult(QueryID_t id, Tupel_t *tupel) {
	struct timeval time;
	//printTupel(slcDataModel,tupel);
	freeTupel(slcDataModel,tupel);
	do_gettimeofday(&time);
	printk("Received tupel with %d items at memory address %p at %lu us\n",tupel->itemLen,tupel,time.tv_sec * USEC_PER_MSEC + time.tv_usec);
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
	SET_PREDICATE(filterTXPredicate,EQUAL, STREAM, "net.packetType.macProtocol", POD, "42")
	INIT_SELECT(selectTest,NULL,1)
	ADD_ELEMENT(selectTest,0,elemPacket,"net.packetType")
}

static void initDatamodel(void) {
	int i = 0;
	INIT_SOURCE_POD(srcSocketType,"type",objSocket,INT,getSrc)
	INIT_SOURCE_POD(srcSocketFlags,"flags",objSocket,INT,getSrc)
	INIT_OBJECT(objSocket,"socket",nsNet,2,INT,activate,deactivate)
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

	INIT_TYPE(typePacketType,"packetType",nsNet,2)
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
	INIT_EVENT_COMPLEX(evtOnRX,"onRx",objDevice,"net.packetType",activate,deactivate)
	INIT_EVENT_COMPLEX(evtOnTX,"onTx",objDevice,"net.packetType",activate,deactivate)

	INIT_OBJECT(objDevice,"device",nsNet,4,STRING,activate,deactivate)
	ADD_CHILD(objDevice,0,srcTXBytes)
	ADD_CHILD(objDevice,1,srcRXBytes)
	ADD_CHILD(objDevice,2,evtOnRX)
	ADD_CHILD(objDevice,3,evtOnTX)
	
	INIT_NS(nsNet,"net",model,3)
	ADD_CHILD(nsNet,0,objDevice)
	ADD_CHILD(nsNet,1,objSocket)
	ADD_CHILD(nsNet,2,typePacketType)
	
	INIT_MODEL(model,1)
	ADD_CHILD(model,0,nsNet)
}

int __init net_init(void)
{
	int ret = 0;
	initDatamodel();
	initQuery();

	if ((ret = registerProvider(&model, &query)) < 0 ) {
		DEBUG_MSG(1,"Register failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Sucessfully registered datamodel and query. Query has id: 0x%x\n",query.queryID);
	DEBUG_MSG(1,"Registered net provider\n");

	return 0;
}

void __exit net_exit(void) {
	int ret = 0;

	if ((ret = unregisterProvider(&model, &query)) < 0 ) {
		DEBUG_MSG(1,"Unregister failed: %d\n",-ret);
	}

	freeQuery(GET_BASE(txStream),0);
	freeDataModel(&model,0);
	DEBUG_MSG(1,"Unregistered net provider\n");
}

module_init(net_init);
module_exit(net_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
