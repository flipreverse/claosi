#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

DECLARE_ELEMENTS(nsNet, model)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen)
static void initDatamodel(void);
static int active = 0;
static LIST_HEAD(rxQueriesList);
static DEFINE_SPINLOCK(rxListLock);
static LIST_HEAD(txQueriesList);
static DEFINE_SPINLOCK(txListLock);


static int handlerTX(struct kprobe *p, struct pt_regs *regs) {
	struct sk_buff *skb = NULL;
	Tupel_t *tupel = NULL;
	struct timeval time;
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	char *devName = NULL;
	unsigned long flags;
	unsigned long long timeUS = 0;

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
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;

	// Acquire the slcLock to avoid change in the datamodel while creating the tuple
	ACQUIRE_READ_LOCK(slcLock);
	spin_lock(&txListLock);
	list_for_each(pos,&txQueriesList) {
		querySelec = container_of(pos,QuerySelectors_t,list);
		if (strcmp(skb->dev->name,GET_SELECTORS(querySelec->query)[0].value) != 0) {
			continue;
		}
		devName = ALLOC(strlen(skb->dev->name) + 1);
		if (devName == NULL) {
			continue;
		}
		strcpy(devName,skb->dev->name);
		tupel = initTupel(timeUS,2);
		if (tupel == NULL) {
			continue;
		}
		allocItem(SLC_DATA_MODEL,tupel,0,"net.device");
		setItemString(SLC_DATA_MODEL,tupel,"net.device",devName);
		allocItem(SLC_DATA_MODEL,tupel,1,"net.packetType");
		setItemArray(SLC_DATA_MODEL,tupel,"net.packetType.macHdr",ETH_HLEN);
		copyArrayByte(SLC_DATA_MODEL,tupel,"net.packetType.macHdr",0,skb->data,ETH_HLEN);
		setItemByte(SLC_DATA_MODEL,tupel,"net.packetType.macProtocol",42);
		eventOccuredUnicast(querySelec->query,tupel);
	}
	spin_unlock(&txListLock);
	RELEASE_READ_LOCK(slcLock);

	return 0;
}

static int handlerRX(struct kprobe *p, struct pt_regs *regs) {
	struct sk_buff *skb = NULL;
	Tupel_t *tupel = NULL;
	struct timeval time;
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	char *devName = NULL;
	unsigned long flags;
	unsigned long long timeUS = 0;

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
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;

	// Acquire the slcLock to avoid change in the datamodel while creating the tuple
	ACQUIRE_READ_LOCK(slcLock);
	spin_lock(&rxListLock);
	list_for_each(pos,&rxQueriesList) {
		querySelec = container_of(pos,QuerySelectors_t,list);
		if (strcmp(skb->dev->name,GET_SELECTORS(querySelec->query)[0].value) != 0) {
			continue;
		}
		devName = ALLOC(strlen(skb->dev->name) + 1);
		if (devName == NULL) {
			continue;
		}
		strcpy(devName,skb->dev->name);
		tupel = initTupel(timeUS,2);
		if (tupel == NULL) {
			continue;
		}
		allocItem(SLC_DATA_MODEL,tupel,0,"net.device");
		setItemString(SLC_DATA_MODEL,tupel,"net.device",devName);
		allocItem(SLC_DATA_MODEL,tupel,1,"net.packetType");
		setItemArray(SLC_DATA_MODEL,tupel,"net.packetType.macHdr",ETH_HLEN);
		copyArrayByte(SLC_DATA_MODEL,tupel,"net.packetType.macHdr",0,skb->data,ETH_HLEN);
		setItemByte(SLC_DATA_MODEL,tupel,"net.packetType.macProtocol",42);
		eventOccuredUnicast(querySelec->query,tupel);
	}
	spin_unlock(&rxListLock);
	RELEASE_READ_LOCK(slcLock);

	return 0;
}

static struct kprobe rxKP = {
	.symbol_name	= "__netif_receive_skb",
	.pre_handler = handlerRX
};

static struct kprobe txKP = {
	.symbol_name	= "dev_hard_start_xmit",
	.pre_handler = handlerTX
};

static void activateTX(Query_t *query) {
	int ret = 0;
	QuerySelectors_t *querySelec = NULL;

	querySelec = (QuerySelectors_t*)ALLOC(sizeof(QuerySelectors_t));
	if (querySelec == NULL) {
		return;
	}
	querySelec->query = query;
	spin_lock(&txListLock);
	list_add_tail(&querySelec->list,&txQueriesList);
	spin_unlock(&txListLock);
	if (active == 0) {
		if ((ret = register_kprobe(&txKP)) < 0) {
			ERR_MSG("register_kprobe failed. Reason: %d\n",ret);
			return;
		}
		active = 1;
	}
	DEBUG_MSG(1,"Registered kprobe at %s\n",txKP.symbol_name);
}

static void deactivateTX(Query_t *query) {
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;

	if (active == 1) {
		unregister_kprobe(&txKP);
		DEBUG_MSG(1,"Unregistered kprobes at %s\n",txKP.symbol_name);
		active = 0;
	}
	spin_lock(&txListLock);
	list_for_each_safe(pos,next,&txQueriesList) {
		querySelec = container_of(pos,QuerySelectors_t,list);
		if (querySelec->query == query) {
			list_del(&querySelec->list);
			break;
		}
	}
	spin_unlock(&txListLock);
}

static void activateRX(Query_t *query) {
	int ret = 0;
	QuerySelectors_t *querySelec = NULL;

	querySelec = (QuerySelectors_t*)ALLOC(sizeof(QuerySelectors_t));
	if (querySelec == NULL) {
		return;
	}
	querySelec->query = query;
	spin_lock(&rxListLock);
	list_add_tail(&querySelec->list,&rxQueriesList);
	spin_unlock(&rxListLock);
	if (active == 0) {
		if ((ret = register_kprobe(&rxKP)) < 0) {
			ERR_MSG("register_kprobe failed. Reason: %d\n",ret);
			return;
		}
		active = 1;
	}
	DEBUG_MSG(1,"Registered kprobe at %s\n",rxKP.symbol_name);
}

static void deactivateRX(Query_t *query) {
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;

	if (active == 1) {
		unregister_kprobe(&rxKP);
		DEBUG_MSG(1,"Unregistered kprobes at %s\n",rxKP.symbol_name);
		active = 0;
	}
	spin_lock(&rxListLock);
	list_for_each_safe(pos,next,&rxQueriesList) {
		querySelec = container_of(pos,QuerySelectors_t,list);
		if (querySelec->query == query) {
			list_del(&querySelec->list);
			break;
		}
	}
	spin_unlock(&rxListLock);
}

static Tupel_t* getRxBytes(Selector_t *selectors, int len) {
	struct net_device *dev = NULL;
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *stats = NULL;
	unsigned long rxBytes = 0;
	Tupel_t *tuple = NULL;
	char *devName = NULL;
	struct timeval time;
	unsigned long long timeUS = 0;
	
	if (len == 0 || selectors == NULL) {
		return NULL;
	}
	dev = dev_get_by_name(&init_net,selectors[0].value);
	if (dev == NULL) {
		return NULL;
	}
	stats = dev_get_stats(dev, &temp);
	rxBytes = stats->rx_bytes;
	devName = ALLOC(strlen(dev->name) + 1);
	if (devName == NULL) {
		dev_put(dev);
		return NULL;
	}
	strcpy(devName,dev->name);
	dev_put(dev);

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	tuple = initTupel(timeUS,2);
	if (tuple == NULL) {
		return NULL;
	}
	allocItem(SLC_DATA_MODEL,tuple,0,"net.device");
	setItemString(SLC_DATA_MODEL,tuple,"net.device",devName);
	allocItem(SLC_DATA_MODEL,tuple,1,"net.device.rxBytes");
	setItemInt(SLC_DATA_MODEL,tuple,"net.device.rxBytes",rxBytes);

	return tuple;
};

static Tupel_t* getTxBytes(Selector_t *selectors, int len) {
	struct net_device *dev = NULL;
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *stats = NULL;
	unsigned long txBytes = 0;
	Tupel_t *tuple = NULL;
	char *devName = NULL;
	struct timeval time;
	unsigned long long timeUS = 0;

	if (len == 0 || selectors == NULL) {
		return NULL;
	}
	dev = dev_get_by_name(&init_net,selectors[0].value);
	if (dev == NULL) {
		return NULL;
	}
	stats = dev_get_stats(dev, &temp);
	txBytes = stats->tx_bytes;
	devName = ALLOC(strlen(dev->name) + 1);
	if (devName == NULL) {
		dev_put(dev);
		return NULL;
	}
	strcpy(devName,dev->name);
	dev_put(dev);

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	tuple = initTupel(timeUS,2);
	if (tuple == NULL) {
		return NULL;
	}
	allocItem(SLC_DATA_MODEL,tuple,0,"net.device");
	setItemString(SLC_DATA_MODEL,tuple,"net.device",devName);
	allocItem(SLC_DATA_MODEL,tuple,1,"net.device.txBytes");
	setItemInt(SLC_DATA_MODEL,tuple,"net.device.txBytes",txBytes);

	return tuple;
}

static void activateDevice(Query_t *query) {
	
}

static void deactivateDevice(Query_t *query) {
	
}

static Tupel_t* generateDeviceStatus(Selector_t *selectors, int len) {
	return NULL;
}

static Tupel_t* getSockType(Selector_t *selectors, int len) {
	return NULL;
}

static Tupel_t* getSockFlags(Selector_t *selectors, int len) {
	return NULL;
}

static void activateSocket(Query_t *query) {
	
}

static void deactivateSocket(Query_t *query) {
	
}

static Tupel_t* generateSocketStatus(Selector_t *selectors, int len) {
	return NULL;
}

static void initDatamodel(void) {
	int i = 0;
	INIT_SOURCE_POD(srcSocketType,"type",objSocket,INT,getSockType)
	INIT_SOURCE_POD(srcSocketFlags,"flags",objSocket,INT,getSockFlags)
	INIT_OBJECT(objSocket,"socket",nsNet,2,INT,activateSocket,deactivateSocket,generateSocketStatus)
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

	INIT_SOURCE_POD(srcTXBytes,"txBytes",objDevice,INT,getTxBytes)
	INIT_SOURCE_POD(srcRXBytes,"rxBytes",objDevice,STRING,getRxBytes)
	INIT_EVENT_COMPLEX(evtOnRX,"onRx",objDevice,"net.packetType",activateRX,deactivateRX)
	INIT_EVENT_COMPLEX(evtOnTX,"onTx",objDevice,"net.packetType",activateTX,deactivateTX)

	INIT_OBJECT(objDevice,"device",nsNet,4,STRING,activateDevice,deactivateDevice,generateDeviceStatus)
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

	if ((ret = registerProvider(&model, NULL)) < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Registered net provider\n");

	return 0;
}

void __exit net_exit(void) {
	int ret = 0;

	if ((ret = unregisterProvider(&model, NULL)) < 0 ) {
		ERR_MSG("Unregister failed: %d\n",-ret);
	}

	freeDataModel(&model,0);
	DEBUG_MSG(1,"Unregistered net provider\n");
}

module_init(net_init);
module_exit(net_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
