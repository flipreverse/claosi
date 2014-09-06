#include <linux/module.h>
#include <linux/relay.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>
#include <evaluation.h>

#define PRINT_TUPLE
#undef PRINT_TUPLE

static EventStream_t rxStream, txStream;
static Predicate_t rxJoinProcessPredicateSocket, rxJoinProcessPredicatePID, txJoinProcessPredicate, txJoinProcessPredicatePID;
static Join_t rxJoinProcess, txJoinProcess;
static Query_t queryRX, queryTX;
static struct rchan *relayfsOutput;
static struct dentry *relayfsDir = NULL;

static void printResultRx(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;
	Sample_t sample;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif
#ifdef PRINT_TUPLE
	printk("Received packet on device %s. Received by process %d. Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"process.process"),getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	sample.ts1 = tupel->timestamp;
#ifdef EVALUATION
	sample.ts2 = tupel->timestamp2;
	sample.ts3 = tupel->timestamp3;
#endif
	sample.ts4 = timeUS;
	relay_write(relayfsOutput,&sample,sizeof(sample));

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTx(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;
	Sample_t sample;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif
#ifdef PRINT_TUPLE
	printk("Transmitted packet on device %s. Sent by process %d. Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"process.process"),getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	sample.ts1 = tupel->timestamp;
#ifdef EVALUATION
	sample.ts2 = tupel->timestamp2;
	sample.ts3 = tupel->timestamp3;
#endif
	sample.ts4 = timeUS;
	relay_write(relayfsOutput,&sample,sizeof(sample));

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryRX);
	queryRX.onQueryCompleted = printResultRx;
	queryRX.root = GET_BASE(rxStream);
	queryRX.next = &queryTX;
	INIT_EVT_STREAM(rxStream,"net.device.onRx",1,0,GET_BASE(rxJoinProcess))
	SET_SELECTOR_STRING(rxStream,0,"eth0")
	INIT_JOIN(rxJoinProcess,"process.process.sockets",NULL,2)
	ADD_PREDICATE(rxJoinProcess,0,rxJoinProcessPredicateSocket)
	SET_PREDICATE(rxJoinProcessPredicateSocket,EQUAL, OP_JOIN, "process.process.sockets", OP_STREAM, "net.packetType.socket")
	ADD_PREDICATE(rxJoinProcess,1,rxJoinProcessPredicatePID)
	SET_PREDICATE(rxJoinProcessPredicatePID,EQUAL, OP_JOIN, "process.process", OP_POD, "-1")

	initQuery(&queryTX);
	queryTX.onQueryCompleted = printResultTx;
	queryTX.root = GET_BASE(txStream);
	INIT_EVT_STREAM(txStream,"net.device.onTx",1,0,GET_BASE(txJoinProcess))
	SET_SELECTOR_STRING(txStream,0,"eth0")
	INIT_JOIN(txJoinProcess,"process.process.sockets",NULL,2)
	ADD_PREDICATE(txJoinProcess,0,txJoinProcessPredicate)
	SET_PREDICATE(txJoinProcessPredicate,EQUAL, OP_JOIN, "process.process.sockets", OP_STREAM, "net.packetType.socket")
	ADD_PREDICATE(txJoinProcess,1,txJoinProcessPredicatePID)
	SET_PREDICATE(txJoinProcessPredicatePID,EQUAL, OP_JOIN, "process.process", OP_POD, "-1")
}

static struct dentry *create_buf_file_handler(const char *filename,
	struct dentry *parent,
	umode_t mode,
	struct rchan_buf *buf,
	int *is_global) {

	return debugfs_create_file(filename, 0777, parent, buf,&relay_file_operations);
}

static int remove_buf_file_handler(struct dentry *dentry) {

	debugfs_remove(dentry);
	return 0;
}

static struct rchan_callbacks relay_callbacks = {
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

int __init evalqueries_init(void) {
	int ret = 0;
	setupQueries();

	relayfsDir = debugfs_create_dir(RELAYFS_DIR,NULL);
	if (relayfsDir == NULL) {
		ERR_MSG("Cannot create debugfs direcotry %s\n",RELAYFS_DIR);
		return -1;
	}
	relayfsOutput = relay_open(RELAYFS_NAME, relayfsDir, SUBBUF_SIZE, N_SUBBUFS,&relay_callbacks,NULL);
	if (relayfsOutput == NULL) {
		ERR_MSG("Cannot create relayfs %s\n",RELAYFS_NAME);
		debugfs_remove(relayfsDir);
		return -1;
	}

	ret = registerQuery(&queryRX);
	if (ret < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Registered eval net queries\n");

	return 0;
}

void __exit evalqueries_exit(void) {
	int ret = 0;

	ret = unregisterQuery(&queryRX);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval net failed: %d\n",-ret);
	}

	relay_close(relayfsOutput);
	debugfs_remove(relayfsDir);

	freeOperator(GET_BASE(rxStream),0);
	freeOperator(GET_BASE(txStream),0);
	DEBUG_MSG(1,"Unregistered eval net queries\n");
}

module_init(evalqueries_init);
module_exit(evalqueries_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
