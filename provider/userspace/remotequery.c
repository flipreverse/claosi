#define MSG_FMT(fmt) "[slc-remotequery] " fmt
#include <query.h>
#include <api.h>
#include <statistic.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define PRINT_TUPLE
//#undef PRINT_TUPLE

#define DEFAULT_DEVICE "eth1"

static EventStream_t rxStreamJoin, txStreamJoin;
static Predicate_t rxJoinProcessPredicateSocket, rxJoinProcessPredicatePID, txJoinProcessPredicate, txJoinProcessPredicatePID;
static Join_t rxJoinProcess, txJoinProcess;
static Query_t queryRXJoin, queryTXJoin;
static char *devName = NULL;

static void printResultRxJoin(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

#ifdef PRINT_TUPLE
	printf("Received packet on device %s. Received by process %d. Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"process.process"),getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTxJoin(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

#ifdef PRINT_TUPLE
	printf("Transmitted packet on device %s. Sent by process %d. Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"process.process"),getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryRXJoin);
	queryRXJoin.onQueryCompleted = printResultRxJoin;
	queryRXJoin.root = GET_BASE(rxStreamJoin);
	queryRXJoin.next = &queryTXJoin;
	INIT_EVT_STREAM(rxStreamJoin,"net.device.onRx",1,0,GET_BASE(rxJoinProcess))
	SET_SELECTOR_STRING(rxStreamJoin,0,devName)
	INIT_JOIN(rxJoinProcess,"process.process.sockets",NULL,2)
	ADD_PREDICATE(rxJoinProcess,0,rxJoinProcessPredicateSocket)
	SET_PREDICATE(rxJoinProcessPredicateSocket,EQUAL, OP_JOIN, "process.process.sockets", OP_STREAM, "net.packetType.socket")
	ADD_PREDICATE(rxJoinProcess,1,rxJoinProcessPredicatePID)
	SET_PREDICATE(rxJoinProcessPredicatePID,EQUAL, OP_JOIN, "process.process", OP_POD, "-1")

	initQuery(&queryTXJoin);
	queryTXJoin.onQueryCompleted = printResultTxJoin;
	queryTXJoin.root = GET_BASE(txStreamJoin);
	INIT_EVT_STREAM(txStreamJoin,"net.device.onTx",1,0,GET_BASE(txJoinProcess))
	SET_SELECTOR_STRING(txStreamJoin,0,devName)
	INIT_JOIN(txJoinProcess,"process.process.sockets",NULL,2)
	ADD_PREDICATE(txJoinProcess,0,txJoinProcessPredicate)
	SET_PREDICATE(txJoinProcessPredicate,EQUAL, OP_JOIN, "process.process.sockets", OP_STREAM, "net.packetType.socket")
	ADD_PREDICATE(txJoinProcess,1,txJoinProcessPredicatePID)
	SET_PREDICATE(txJoinProcessPredicatePID,EQUAL, OP_JOIN, "process.process", OP_POD, "-1")
}

int onLoad(int argc, char *argv[]) {
	int ret = 0, opt;

	optind = 1;
	opterr = 0;
	while ((opt = getopt(argc, argv, "d:")) != -1) {
		switch (opt) {
		case 'd':
			devName = strdup(optarg);
			if (devName == NULL) {
				ERR_MSG("Cannot allocate memory for devName\n");
				return -1;
			}
			break;
		default:
			ERR_MSG("Usage: %s [-d <device>]\n", __FILE__);
			return -1;
		}
	}
	if (devName == NULL) {
		devName = strdup(DEFAULT_DEVICE);
	}
	DEBUG_MSG(1, "devName=%s\n", devName);

	setupQueries();

	if ((ret = registerQuery(&queryRXJoin)) < 0 ) {
		ERR_MSG("Query registration failed: %d\n",-ret);
		return -1;
	}

	return 0;
}

int onUnload(void) {
	int ret = 0;

	if ((ret = unregisterQuery(&queryRXJoin)) < 0 ) {
		ERR_MSG("Unregister queries failed: %d\n",-ret);
	}

	free(devName);
	freeOperator(GET_BASE(rxStreamJoin),0);
	freeOperator(GET_BASE(txStreamJoin),0);

	return 0;
}
