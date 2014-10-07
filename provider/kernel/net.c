#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/inet_hashtables.h>
#include <net/sock.h>
#include <net/udp.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

DECLARE_ELEMENTS(nsNet, model)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen, typeSockRef)

DECLARE_QUERY_LIST(rx);
DECLARE_QUERY_LIST(tx);
DECLARE_QUERY_LIST(dev);

static char rxSymbolNameTCP[] = "tcp_v4_rcv";
static char rxSymbolNameUCP[] = "udp_rcv";
static struct kprobe rxKPTCP, rxKPUDP, *rxKP[2];
static char txSymbolName[] = "dev_hard_start_xmit";
static struct kprobe txKP;

static int handlerTX(struct kprobe *p, struct pt_regs *regs) {
	struct sk_buff *skb = NULL;
	Tupel_t *tupel = NULL;
#ifndef EVALUATION
	struct timeval time;
#endif
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	char *devName = NULL;
	unsigned long flags;
	unsigned long long timeUS = 0;

#if defined(__i386__)
skb = (struct sk_buff*)regs->ax;
#elif defined(__x86_64__)
skb = (struct sk_buff*)regs->di;
#elif defined(__arm__)
skb = (struct sk_buff*)regs->ARM_r0;
#else
#error Unknown architecture
#endif

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	// Was the packet received by a device a query was registered on?
	forEachQueryEvent(slcLock,tx,pos,querySelec)
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
		setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength",skb->len);
		if (skb->sk && skb->sk->sk_socket) {
			setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.socket",SOCK_INODE(skb->sk->sk_socket)->i_ino);
		} else {
			setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.socket",-1);
		}
		eventOccuredUnicast(querySelec->query,tupel);
	endForEachQueryEvent(slcLock,tx)

	return 0;
}

static int handlerRX(struct kprobe *p, struct pt_regs *regs) {
	struct sk_buff *skb = NULL;
	struct sock *sk = NULL;
	const struct tcphdr *th = NULL;
	struct iphdr *iph;
	struct udphdr *uh = NULL;
	Tupel_t *tupel = NULL;
#ifndef EVALUATION
	struct timeval time;
#endif
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	char *devName = NULL;
	unsigned long flags;
	unsigned long long timeUS = 0;

#if defined(__i386__)
skb = (struct sk_buff*)regs->ax;
#elif defined(__x86_64__)
skb = (struct sk_buff*)regs->di;
#elif defined(__arm__)
skb = (struct sk_buff*)regs->ARM_r0;
#else
#error Unknown architecture
#endif
	/*
	 * Initially, an instance of struct sk_buff does *not* have a socket set.
	 * It will be resolved while being passed through the diffrent layers.
	 * For example, the functions (tcp_v4_rcv/udp_rcv) we're probing do this job.
	 * Hence, it's necessary to do the same stuff here.
	 */
	if (p == &rxKPTCP) {
		// TCP packet
		th = tcp_hdr(skb);
		sk = __inet_lookup_skb(&tcp_hashinfo, skb, th->source, th->dest);
	} else if (p == &rxKPUDP) {
		// UDP packet
		iph = ip_hdr(skb);
		uh = udp_hdr(skb);
		sk = __udp4_lib_lookup(dev_net(skb_dst(skb)->dev), iph->saddr, uh->source,iph->daddr, uh->dest, inet_iif(skb),&udp_table);
	}
	// No valid socket found. Abort.
	if (sk == NULL) {
		return 0;
	}

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif
	// Acquire the slcLock to avoid change in the datamodel while creating the tuple
	forEachQueryEvent(slcLock,rx,pos,querySelec)
		// Was the packet received by a device a query was registered on?
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
		setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength",skb->len);
		if (sk && sk->sk_socket) {
			setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.socket",SOCK_INODE(sk->sk_socket)->i_ino);
		} else {
			setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.socket",-1);
		}
		eventOccuredUnicast(querySelec->query,tupel);
	endForEachQueryEvent(slcLock,rx)
	// Give the socket back to the kernel
	sock_put(sk);

	return 0;
}

static void activateTX(Query_t *query) {
	int ret = 0;
	QuerySelectors_t *querySelec = NULL;

	addAndEnqueueQuery(tx,ret, querySelec, query)
	// list was empty before insertion
	if (ret == 1) {
		memset(&txKP,0,sizeof(struct kprobe));
		txKP.pre_handler = handlerTX;
		txKP.symbol_name = txSymbolName;
		ret = register_kprobe(&txKP);
		if (ret < 0) {
			ERR_MSG("register_kprobe at %s failed. Reason: %d\n",txKP.symbol_name,ret);
			return;
		}
		DEBUG_MSG(1,"Registered kprobe at %s\n",txKP.symbol_name);
	}
}

static void deactivateTX(Query_t *query) {
	int ret = 0;
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;

	findAndDeleteQuery(tx,ret, querySelec, query, pos, next)
	// list is now empty
	if (ret == 1) {
		unregister_kprobe(&txKP);
		DEBUG_MSG(1,"Unregistered kprobe at %s. Missed it %ld times.\n",txKP.symbol_name,txKP.nmissed);
	}
}

static void activateRX(Query_t *query) {
	int ret = 0;
	QuerySelectors_t *querySelec = NULL;

	addAndEnqueueQuery(rx,ret, querySelec, query)
	// list was empty before insertion
	if (ret == 1) {
		/*
		 * Initially, an instance of struct sk_buff does *not* have a socket set.
		 * It will be resolved while being passed through the diffrent layers.
		 * The ipv4 variants of tcp and udp resolve the socket in tcp_v4_rcv and udp_rcv respectively.
		 * Thus, it is insuffiecient to register a kprobe for packet reception which would be netif_receive_skb().
		 * Furthermore, two kprobes for both functions tcp and udp are necessary.
		 */
		memset(&rxKPTCP,0,sizeof(struct kprobe));
		memset(&rxKPUDP,0,sizeof(struct kprobe));
		rxKPTCP.pre_handler = handlerRX;
		rxKPTCP.symbol_name = rxSymbolNameTCP;
		rxKP[0] = &rxKPTCP;
		rxKPUDP.pre_handler = handlerRX;
		rxKPUDP.symbol_name = rxSymbolNameUCP;
		rxKP[1] = &rxKPUDP;
		ret = register_kprobes(rxKP,2);
		if (ret < 0) {
			ERR_MSG("register_kprobe at %s and %s failed. Reason: %d\n",rxKPTCP.symbol_name,rxKPUDP.symbol_name,ret);
			return;
		}
		DEBUG_MSG(1,"Registered kprobe at %s and %s\n",rxKPTCP.symbol_name,rxKPUDP.symbol_name);
	}
}

static void deactivateRX(Query_t *query) {
	int ret = 0;
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;

	findAndDeleteQuery(rx,ret, querySelec, query, pos, next)
	// list is now empty
	if (ret == 1) {
		unregister_kprobes(rxKP,2);
		DEBUG_MSG(1,"Unregistered kprobe at %s (missed=%ld) and %s (missed=%ld).\n",rxKPTCP.symbol_name,rxKPTCP.nmissed,rxKPUDP.symbol_name,rxKPUDP.nmissed);
	}
}

static Tupel_t* getRxBytes(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	struct net_device *dev = NULL;
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *stats = NULL;
	unsigned long rxBytes = 0;
	Tupel_t *tuple = NULL;
	char *devName = NULL;
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;
	
	if (len == 0 || selectors == NULL) {
		return NULL;
	}
	// the user has to provide one selector which is the device name
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
	// Give the device back to the kernel
	dev_put(dev);

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

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

static Tupel_t* getTxBytes(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	struct net_device *dev = NULL;
	struct rtnl_link_stats64 temp;
	const struct rtnl_link_stats64 *stats = NULL;
	unsigned long txBytes = 0;
	Tupel_t *tuple = NULL;
	char *devName = NULL;
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;

	if (len == 0 || selectors == NULL) {
		return NULL;
	}
	// the user has to provide one selector which is the device name
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
	// Give the device back to the kernel
	dev_put(dev);

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

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

static int handlerOpen(struct kprobe *p, struct pt_regs *regs) {
	struct net_device *dev = NULL;
	Tupel_t *tupel = NULL;
#ifndef EVALUATION
	struct timeval time;
#endif
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	GenStream_t *stream = NULL;
	char *devName = NULL;
	unsigned long flags;
	unsigned long long timeUS = 0;

#if defined(__i386__)
dev = (struct net_device*)regs->ax;
#elif defined(__x86_64__)
dev = (struct net_device*)regs->ax;
#elif defined(__arm__)
dev = (struct net_device*)regs->ARM_r0;
#else
#error Unknown architecture
#endif

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	// Acquire the slcLock to avoid change in the datamodel while creating the tuple
	forEachQueryObject(slcLock, dev, pos, querySelec, OBJECT_CREATE)
		stream = ((GenStream_t*)querySelec->query->root);
		// Consider the selector the user may provide
		if (stream->selectorsLen > 0) {
			if (strcmp(dev->name,GET_SELECTORS(querySelec->query)[0].value) != 0) {
				continue;
			}
		}
		devName = ALLOC(strlen(dev->name) + 1);
		if (devName == NULL) {
			continue;
		}
		strcpy(devName,dev->name);
		tupel = initTupel(timeUS,1);
		if (tupel == NULL) {
			continue;
		}
		allocItem(SLC_DATA_MODEL,tupel,0,"net.device");
		setItemString(SLC_DATA_MODEL,tupel,"net.device",devName);
		objectChangedUnicast(querySelec->query,tupel);
	endForEachQueryEvent(slcLock,dev)

	return 0;
}

static int handlerClose(struct kprobe *p, struct pt_regs *regs) {
	struct net_device *dev = NULL;
	Tupel_t *tupel = NULL;
#ifndef EVALUATION
	struct timeval time;
#endif
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	GenStream_t *stream = NULL;
	char *devName = NULL;
	unsigned long flags;
	unsigned long long timeUS = 0;

#if defined(__i386__)
dev = (struct net_device*)regs->ax;
#elif defined(__x86_64__)
dev = (struct net_device*)regs->ax;
#elif defined(__arm__)
dev = (struct net_device*)regs->ARM_r0;
#else
#error Unknown architecture
#endif

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	// Acquire the slcLock to avoid change in the datamodel while creating the tuple
	forEachQueryObject(slcLock, dev, pos, querySelec, OBJECT_DELETE)
		stream = ((GenStream_t*)querySelec->query->root);
		// Consider the selector the user may provide
		if (stream->selectorsLen > 0) {
			if (strcmp(dev->name,GET_SELECTORS(querySelec->query)[0].value) != 0) {
				continue;
			}
		}
		devName = ALLOC(strlen(dev->name) + 1);
		if (devName == NULL) {
			continue;
		}
		strcpy(devName,dev->name);
		tupel = initTupel(timeUS,1);
		if (tupel == NULL) {
			continue;
		}
		allocItem(SLC_DATA_MODEL,tupel,0,"net.device");
		setItemString(SLC_DATA_MODEL,tupel,"net.device",devName);
		eventOccuredUnicast(querySelec->query,tupel);
	endForEachQueryEvent(slcLock,dev)

	return 0;
}

static struct kprobe devOpenKP = {
	.symbol_name	= "dev_open",
	.pre_handler = handlerOpen
};

static struct kprobe devCloseKP = {
	.symbol_name	= "dev_close",
	.pre_handler = handlerClose
};

static void activateDevice(Query_t *query) {
	int ret = 0;
	QuerySelectors_t *querySelec = NULL;

	addAndEnqueueQuery(dev,ret, querySelec, query)
	// list was empty
	if (ret == 1) {
		ret = register_kprobe(&devOpenKP);
		if (ret < 0) {
			ERR_MSG("register_kprobe at %s failed. Reason: %d\n",devOpenKP.symbol_name,ret);
		}
		ret = register_kprobe(&devCloseKP);
		if (ret < 0) {
			ERR_MSG("register_kprobe at %s failed. Reason: %d\n",devCloseKP.symbol_name,ret);
		}
		DEBUG_MSG(1,"Registered kprobe at %s and %s\n",devOpenKP.symbol_name,devCloseKP.symbol_name);
	}
}

static void deactivateDevice(Query_t *query) {
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;
	int listEmpty = 0;

	findAndDeleteQuery(dev,listEmpty, querySelec, query, pos, next)
	if (listEmpty == 1) {
		unregister_kprobe(&devOpenKP);
		unregister_kprobe(&devCloseKP);
		DEBUG_MSG(1,"Unregistered kprobes at %s (missed=%ld) and %s (missed=%ld)\n",devOpenKP.symbol_name,devOpenKP.nmissed,devCloseKP.symbol_name,devCloseKP.nmissed);
	}
}

static Tupel_t* generateDeviceStatus(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	struct net_device *curDev = NULL;
	Tupel_t *head = NULL, *curTuple = NULL, *prevTuple = NULL;
	char *devName = NULL;
	#ifndef EVALUATION
	struct timeval time;
	#endif
	unsigned long long timeUS = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	for_each_netdev(&init_net, curDev) {
		devName = ALLOC(strlen(curDev->name) + 1);
		if (devName == NULL) {
			continue;
		}
		curTuple = initTupel(timeUS,1);
		if (curTuple == NULL) {
			continue;
		}
		if (head == NULL) {
			head = curTuple;
		}
		strcpy(devName,curDev->name);
		allocItem(SLC_DATA_MODEL,curTuple,0,"net.device");
		setItemString(SLC_DATA_MODEL,curTuple,"net.device",devName);
		if (prevTuple != NULL) {
			prevTuple->next = curTuple;
		}
		prevTuple = curTuple;
	}

	return head;
}

static Tupel_t* getSockType(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	return NULL;
}

static Tupel_t* getSockFlags(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	return NULL;
}

static void activateSocket(Query_t *query) {
	
}

static void deactivateSocket(Query_t *query) {
	
}

static Tupel_t* generateSocketStatus(Selector_t *selectors, int len, Tupel_t* leftTuple) {
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
	INIT_PLAINTYPE(typeDataLen,"dataLength",typePacketType,INT)
	INIT_REF(typeSockRef,"socket",typePacketType,"net.socket")

	INIT_COMPLEX_TYPE(typePacketType,"packetType",nsNet,4)
	ADD_CHILD(typePacketType,0,typeMacHdr);
	ADD_CHILD(typePacketType,1,typeMacProt);
	/*ADD_CHILD(typePacketType,2,typeNetHdr);
	ADD_CHILD(typePacketType,3,typeNetProt);
	ADD_CHILD(typePacketType,4,typeTranspHdr);
	ADD_CHILD(typePacketType,5,typeTransProt);*/
	ADD_CHILD(typePacketType,2,typeDataLen);
	ADD_CHILD(typePacketType,3,typeSockRef);

	INIT_SOURCE_POD(srcTXBytes,"txBytes",objDevice,INT,getTxBytes)
	INIT_SOURCE_POD(srcRXBytes,"rxBytes",objDevice,INT,getRxBytes)
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

	ret = registerProvider(&model, NULL);
	if (ret < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		freeDataModel(&model,0);
		return -1;
	}
	DEBUG_MSG(1,"Registered net provider\n");

	return 0;
}

void __exit net_exit(void) {
	int ret = 0;

	ret = unregisterProvider(&model, NULL);
	if (ret < 0 ) {
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
