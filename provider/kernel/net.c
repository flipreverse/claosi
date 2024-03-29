#define MSG_FMT(fmt)	"[slc-net] " fmt
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/tcp.h>
#include <linux/tracepoint.h>
#include <net/tcp.h>
#include <net/tcp_states.h>
#include <net/inet_hashtables.h>
#include <net/request_sock.h>
#include <net/sock.h>
#include <net/udp.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

#define RX_TRACEPOINT_TCP "tcp_rx"
#define RX_TRACEPOINT_UDP "udp_rx"
#define RX_TRACEPOINT "netif_receive_skb"
#define TX_TRACEPOINT "net_dev_start_xmit"

#define RX_SYMBOL_NAME_TCP "tcp_v4_rcv"
#define RX_SYMBOL_NAME_UDP "udp_rcv"
#define RX_SYMBOL_NAME "__netif_receive_skb_core"
#define TX_SYMBOL_NAME "dev_hard_start_xmit"

DECLARE_ELEMENTS(nsNet, model)
DECLARE_ELEMENTS(objSocket, objDevice, srcSocketType, srcSocketFlags, typePacketType, srcTXBytes, srcRXBytes, evtOnRX, evtOnTX)
DECLARE_ELEMENTS(typeMacHdr, typeMacProt, typeNetHdr, typeNetProt, typeTranspHdr, typeTransProt, typeDataLen, typeSockRef)

DECLARE_QUERY_LIST(rx);
DECLARE_QUERY_LIST(tx);
DECLARE_QUERY_LIST(dev);

static struct kprobe rxKPTCP, rxKPUDP, *rxKP[2], rxKPGeneric;
static struct kprobe txKP;

static struct tracepoint *tpRX = NULL, *tpRXTCP = NULL, *tpRXUDP = NULL;
static struct tracepoint *tpTX = NULL;

static bool useTracepoints = 0;
module_param(useTracepoints, bool, 0644);
MODULE_PARM_DESC(useTracepoints, "Use tracepoints instead of kprobes [default: 0]");

static bool useProtSpecific = 0;
module_param(useProtSpecific, bool, 0644);
MODULE_PARM_DESC(useProtSpecific, "Use protocol-specific rx probes/tp [default: 0]");

static void handlerTX(struct sk_buff *skb) {
	Tupel_t *tupel = NULL;
#ifndef EVALUATION
	struct timeval time;
#endif
	struct sock *sk = NULL;
	struct request_sock *reqsk = NULL;
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	char *devName = NULL;
	unsigned long long timeUS = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	sk = skb->sk;
	if (ntohs(skb->protocol) == ETH_P_IP) {
		struct iphdr *iphd = ip_hdr(skb);
		if (iphd) {
			if (iphd->protocol == IPPROTO_TCP) {
				if (sk && sk->sk_state == TCP_NEW_SYN_RECV) {
					/*
					 * Listening sockets are represented by a special socket struct, i.e., struct request_socket.
					 * This is returned by the above lookup functions.
					 * Thus, we cannot use it to determine the inode number directly.
					 * We first have to resolve the actual struct sock behind it.
					 */
					reqsk = inet_reqsk(sk);
					sk = reqsk->rsk_listener;
				}
			} else if (iphd->protocol == IPPROTO_UDP) {
				// Nothing to do
			} else {
				DEBUG_MSG(2, "Got non TCP/UDP packet\n");
				return;
			}
		} else {
			printk("iphdr is NULL: 0x%llx,0x%llx\n", (uint64_t)skb->head, (uint64_t)skb->data);
		}
	} else {
		DEBUG_MSG(2, "Got non IP packet\n");
		return;
	}

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
		if (sk && sk->sk_socket) {
			setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.socket",SOCK_INODE(sk->sk_socket)->i_ino);
		} else {
			setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.socket",-1);
		}
		eventOccuredUnicast(querySelec->query,tupel);
	endForEachQuery(slcLock,tx);
}

static int kprobeHandlerTX(struct kprobe *p, struct pt_regs *regs) {
	struct sk_buff *skb = NULL;

#if defined(__i386__)
skb = (struct sk_buff*)regs->ax;
#elif defined(__x86_64__)
skb = (struct sk_buff*)regs->di;
#elif defined(__arm__)
skb = (struct sk_buff*)regs->ARM_r0;
#else
#error Unknown architecture
#endif

	handlerTX(skb);
	return 0;
}

static void traceHandlerTX(void *data, struct sk_buff *skb, const struct net_device *dev) {
	handlerTX(skb);
}

static void handlerRX(struct sk_buff *skb) {
	struct sock *sk = NULL;
	struct request_sock *reqsk = NULL;
	const struct tcphdr *th = NULL;
	struct udphdr *uh = NULL;
	Tupel_t *tupel = NULL;
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,7,0)
	bool refcounted = false;
#endif
#ifndef EVALUATION
	struct timeval time;
#endif
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	char *devName = NULL;
	unsigned long long timeUS = 0;

	/*
	 * Initially, an instance of struct sk_buff does *not* have a socket set.
	 * It will be resolved while being passed through the diffrent layers.
	 * For example, the functions (tcp_v4_rcv/udp_rcv) we're probing do this job.
	 * Hence, it's necessary to do the same stuff here.
	 */
	if (ntohs(skb->protocol) == ETH_P_IP) {
		struct iphdr *iph = ip_hdr(skb);
		if (skb->transport_header == 0 || skb->network_header == 0) {
			ERR_MSG("Either network or transport header is 0\n");
			return;
		}
		if (iph) {
			if (iph->protocol == IPPROTO_TCP) {
				/*
				 * TCP packet
				 * Ensure that the transport header has a proper value, i.e., points to the 
				 * data of the ip packet.
				 * ip_rcv()@net/ipv4/ip_input.c:484
				 */
				if ((skb->transport_header - skb->network_header) != (iph->ihl * 4)) {
					ERR_MSG("skb->transport_header does not have a proper value\n");
					return;
				}
				th = tcp_hdr(skb);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0)
				sk = __inet_lookup(dev_net(skb->dev), &tcp_hashinfo, skb, iph->saddr, th->source, iph->daddr, th->dest, inet_iif(skb));
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
				sk = __inet_lookup(dev_net(skb->dev), &tcp_hashinfo, skb, __tcp_hdrlen(th), iph->saddr, th->source, iph->daddr, th->dest, inet_iif(skb));
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
				sk = __inet_lookup(dev_net(skb->dev), &tcp_hashinfo, skb, __tcp_hdrlen(th), iph->saddr, th->source, iph->daddr, th->dest, inet_iif(skb), &refcounted);
#else
				sk = __inet_lookup(dev_net(skb->dev), &tcp_hashinfo, skb, __tcp_hdrlen(th), iph->saddr, th->source, iph->daddr, th->dest, inet_iif(skb), inet_sdif(skb), &refcounted);
#endif
				if (sk == NULL) {
					ERR_MSG("TCP socket not resolved (source=%hu, dest=%hu)!\n", ntohs(th->source), ntohs(th->dest));
					return;
				}
				if (sk->sk_state == TCP_NEW_SYN_RECV) {
					/*
					 * Listening sockets are represented by a special socket struct, i.e., struct request_socket.
					 * This is returned by the above lookup functions.
					 * Thus, we cannot use it to determine the inode number directly.
					 * We first have to resolve the actual struct sock behind it.
					 */
					reqsk = inet_reqsk(sk);
					sk = reqsk->rsk_listener;
				} else if (sk->sk_state == TCP_TIME_WAIT) {
					/*
					 * Sockets in TIME_WAIT state are no real sockets in terms of associated inodes.
					 * Hence, we cannot determine an inode number that we can pass on.
					 */
					return;
				}
			} else if (iph->protocol == IPPROTO_UDP) {
				// UDP packet
				uh = udp_hdr(skb);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,5,0)
				sk = __udp4_lib_lookup(dev_net(skb->dev), iph->saddr, uh->source,iph->daddr, uh->dest, inet_iif(skb),&udp_table);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
				sk = __udp4_lib_lookup(dev_net(skb->dev), iph->saddr, uh->source,iph->daddr, uh->dest, inet_iif(skb),&udp_table, skb);
#else
				sk = __udp4_lib_lookup(dev_net(skb->dev), iph->saddr, uh->source,iph->daddr, uh->dest, inet_iif(skb), inet_sdif(skb), &udp_table, skb);
#endif
			} else {
				DEBUG_MSG(2, "Got non TCP/UDP packet\n");
				return;
			}
		} else {
			ERR_MSG("ip header is NULL: head=0x%llx, data=0x%llx\n", (uint64_t)skb->head, (uint64_t)skb->data);
		}
	} else {
		DEBUG_MSG(2, "Got non IP packet\n");
		return;
	}
	// No valid socket found. Abort.
	if (sk == NULL) {
		return;
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
			setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.socket", SOCK_INODE(sk->sk_socket)->i_ino);
		} else {
			setItemInt(SLC_DATA_MODEL,tupel,"net.packetType.socket",-1);
		}
		eventOccuredUnicast(querySelec->query,tupel);
	endForEachQuery(slcLock,rx)

	// Give the socket back to the kernel
#if LINUX_VERSION_CODE > KERNEL_VERSION(4,7,0)
	if (th != NULL) {
		// Got a tcp socket. Has its refcount been incremented?
		if (refcounted) {
			if (reqsk != NULL) {
				reqsk_put(reqsk);
			} else {
				sock_put(sk);
			}
		}
	} else if (uh != NULL) {
		/*
		 * Got an UDP packet.
		 * The UDP lookup function does *not* increment
		 * the refcount unless udp4_lib_lookup() is used.
		 */
		//sock_put(sk);
	} else {
		ERR_MSG("This should not happen!\n");
	}
#else
	sock_put(sk);
#endif
}

static int kprobeHandlerRX(struct kprobe *p, struct pt_regs *regs) {
	struct sk_buff *skb = NULL;

#if defined(__i386__)
skb = (struct sk_buff*)regs->ax;
#elif defined(__x86_64__)
skb = (struct sk_buff*)regs->di;
#elif defined(__arm__)
skb = (struct sk_buff*)regs->ARM_r0;
#else
#error Unknown architecture
#endif

	handlerRX(skb);
	return 0;
}

static void traceHandlerRX(void *data, struct sk_buff *skb) {
	handlerRX(skb);
}

static void activateTX(Query_t *query) {
	int ret = 0;
	QuerySelectors_t *querySelec = NULL;

	addAndEnqueueQuery(tx,ret, querySelec, query)
	// list was empty before insertion
	if (ret == 1) {
		if (useTracepoints) {
			ret = tracepoint_probe_register(tpTX, traceHandlerTX, NULL);
			if (ret < 0) {
				ERR_MSG("tracepoint_probe_register at %s failed. Reason: %d\n", tpTX->name, ret);
				return;
			}
			INFO_MSG("Registered tracepoint at %s\n",tpTX->name);
		} else {
			memset(&txKP,0,sizeof(struct kprobe));
			txKP.pre_handler = kprobeHandlerTX;
			txKP.symbol_name = TX_SYMBOL_NAME;
			ret = register_kprobe(&txKP);
			if (ret < 0) {
				ERR_MSG("register_kprobe at %s failed. Reason: %d\n",txKP.symbol_name,ret);
				return;
			}
			INFO_MSG("Registered kprobe at %s\n",txKP.symbol_name);
		}
	}
}

static void deactivateTX(Query_t *query) {
	int ret = 0;
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;

	findAndDeleteQuery(tx,ret, querySelec, query, pos, next)
	// list is now empty
	if (ret == 1) {
		if (useTracepoints) {
			ret = tracepoint_probe_unregister(tpTX, traceHandlerTX, NULL);
			if (ret < 0) {
				ERR_MSG("tracepoint_probe_unregister at %s failed. Reason: %d\n", tpTX->name, ret);
				return;
			}
			INFO_MSG("Unregistered tracepoint at %s\n", tpTX->name);
			tracepoint_synchronize_unregister();
		} else {
			unregister_kprobe(&txKP);
			INFO_MSG("Unregistered kprobe at %s. Missed it %ld times.\n",txKP.symbol_name,txKP.nmissed);
		}
	}
}

static void activateRX(Query_t *query) {
	int ret = 0;
	QuerySelectors_t *querySelec = NULL;

	addAndEnqueueQuery(rx,ret, querySelec, query)
	// list was empty before insertion
	if (ret == 1) {
		if (useTracepoints) {
			if (useProtSpecific && tpRXTCP != NULL && tpRXUDP != NULL) {
				ret = tracepoint_probe_register(tpRXTCP, traceHandlerRX, NULL);
				if (ret < 0) {
					ERR_MSG("tracepoint_probe_register at %s failed. Reason: %d\n", tpRXTCP->name, ret);
					return;
				}
				ret = tracepoint_probe_register(tpRXUDP, traceHandlerRX, NULL);
				if (ret < 0) {
					tracepoint_probe_unregister(tpRXTCP, traceHandlerRX, NULL);
					ERR_MSG("tracepoint_probe_register at %s failed. Reason: %d\n", tpRXUDP->name, ret);
					return;
				}
				INFO_MSG("Registered tracepoint at %s and %s\n",tpRXTCP->name, tpRXUDP->name);
			} else {
				if (useProtSpecific) {
					ERR_MSG("No protocol-specific tracepoints present. Falling back to the generic one\n");
				}
				ret = tracepoint_probe_register(tpRX, traceHandlerRX, NULL);
				if (ret < 0) {
					ERR_MSG("tracepoint_probe_register at %s failed. Reason: %d\n", tpRX->name, ret);
					return;
				}
				INFO_MSG("Registered tracepoint at %s\n",tpRX->name);
			}
		} else {
			if (useProtSpecific) {
				/*
				 * Initially, an instance of struct sk_buff does *not* have a socket set.
				 * It will be resolved while being passed through the diffrent layers.
				 * The ipv4 variants of tcp and udp resolve the socket in tcp_v4_rcv and udp_rcv respectively.
				 * Thus, it is insuffiecient to register a kprobe for packet reception which would be netif_receive_skb().
				 * Furthermore, two kprobes for both functions tcp and udp are necessary.
				 */
				memset(&rxKPTCP,0,sizeof(struct kprobe));
				memset(&rxKPUDP,0,sizeof(struct kprobe));
				rxKPTCP.pre_handler = kprobeHandlerRX;
				rxKPTCP.symbol_name = RX_SYMBOL_NAME_TCP;
				rxKP[0] = &rxKPTCP;
				rxKPUDP.pre_handler = kprobeHandlerRX;
				rxKPUDP.symbol_name = RX_SYMBOL_NAME_UDP;
				rxKP[1] = &rxKPUDP;
				ret = register_kprobes(rxKP,2);
				if (ret < 0) {
					ERR_MSG("register_kprobe at %s and %s failed. Reason: %d\n",rxKPTCP.symbol_name,rxKPUDP.symbol_name,ret);
					return;
				}
				INFO_MSG("Registered kprobe at %s and %s\n",rxKPTCP.symbol_name,rxKPUDP.symbol_name);
			} else {
				memset(&rxKPGeneric,0,sizeof(struct kprobe));
				rxKPGeneric.pre_handler = kprobeHandlerRX;
				rxKPGeneric.symbol_name = RX_SYMBOL_NAME;
				ret = register_kprobe(&rxKPGeneric);
				if (ret < 0) {
					ERR_MSG("register_kprobe at %s failed. Reason: %d\n",rxKPGeneric.symbol_name,ret);
					return;
				}
				INFO_MSG("Registered kprobe at %s\n",rxKPGeneric.symbol_name);
			}
		}
	}
}

static void deactivateRX(Query_t *query) {
	int ret = 0;
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;

	findAndDeleteQuery(rx,ret, querySelec, query, pos, next)
	// list is now empty
	if (ret == 1) {
		if (useTracepoints) {
			if (useProtSpecific && tpRXTCP != NULL && tpRXUDP != NULL) {
				ret = tracepoint_probe_unregister(tpRXTCP, traceHandlerRX, NULL);
				if (ret < 0) {
					ERR_MSG("tracepoint_probe_unregister at %s failed. Reason: %d\n", tpRXTCP->name, ret);
				}
				ret = tracepoint_probe_unregister(tpRXUDP, traceHandlerRX, NULL);
				if (ret < 0) {
					ERR_MSG("tracepoint_probe_unregister at %s failed. Reason: %d\n", tpRXUDP->name, ret);
				}
				INFO_MSG("Unregistered tracepoint at %s and %s\n", tpRXTCP->name, tpRXUDP->name);
			} else {
				ret = tracepoint_probe_unregister(tpRX, traceHandlerRX, NULL);
				if (ret < 0) {
					ERR_MSG("tracepoint_probe_unregister at %s failed. Reason: %d\n", tpRX->name, ret);
					return;
				}
				INFO_MSG("Unregistered tracepoint at %s\n", tpRX->name);
			}
			tracepoint_synchronize_unregister();
		} else {
			if (useProtSpecific) {
				unregister_kprobes(rxKP,2);
				INFO_MSG("Unregistered kprobe at %s (missed=%ld) and %s (missed=%ld).\n",rxKPTCP.symbol_name,rxKPTCP.nmissed,rxKPUDP.symbol_name,rxKPUDP.nmissed);
			} else {
				unregister_kprobe(&rxKPGeneric);
				INFO_MSG("Unregistered kprobe at %s (missed=%ld).\n",rxKPGeneric.symbol_name,rxKPGeneric.nmissed);
			}
		}
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
	endForEachQuery(slcLock,dev)

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
	endForEachQuery(slcLock,dev)

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

static void resolveTPs(struct tracepoint *tp, void *data) {
	int *ret = (int*)data;

	if (strcmp(tp->name, RX_TRACEPOINT) == 0) {
		*ret = *ret - 1;
		tpRX = tp;
	} else if (strcmp(tp->name, TX_TRACEPOINT) == 0) {
		*ret = *ret - 1;
		tpTX = tp;
	} else if (strcmp(tp->name, RX_TRACEPOINT_UDP) == 0) {
		tpRXUDP = tp;
	} else if (strcmp(tp->name, RX_TRACEPOINT_TCP) == 0) {
		tpRXTCP = tp;
	}
}

int __init net_init(void)
{
	int ret = 0;
	initDatamodel();

	ret = 2;
	for_each_kernel_tracepoint(resolveTPs, &ret);
	if (ret != 0) {
		ERR_MSG("Cannot resolve tracepoints (%d)\n", ret);
		return -1;
	} else {
		DEBUG_MSG(1, "Resolved all tracepoints\n");
	}

	ret = registerProvider(&model, NULL);
	if (ret < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		freeDataModel(&model,0);
		return -1;
	}
	INFO_MSG("Registered net provider\n");

	return 0;
}

void __exit net_exit(void) {
	int ret = 0;

	ret = unregisterProvider(&model, NULL);
	if (ret < 0 ) {
		ERR_MSG("Unregister failed: %d\n",-ret);
	}

	freeDataModel(&model,0);
	INFO_MSG("Unregistered net provider\n");
}

module_init(net_init);
module_exit(net_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
