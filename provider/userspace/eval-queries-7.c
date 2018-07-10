#define MSG_FMT(fmt) "[slc-eval7] " fmt
#include <datamodel.h>
#include <query.h>
#include <api.h>
#include <evalwriter.h>
#include <sys/time.h>

#define PRINT_TUPLE
#undef PRINT_TUPLE

#define DEFAULT_DEVICE "eth1"

#define OUTPUT_FILENAME "delay-slc-7.txt"

static EventStream_t rxStreamJoin, txStreamJoin;
static Predicate_t rxJoinProcessPredicateSocket, rxJoinProcessPredicatePID, txJoinProcessPredicate, txJoinProcessPredicatePID;
static Join_t rxJoinProcess, txJoinProcess;
static Query_t queryRXJoin, queryTXJoin;
static char *devName = NULL;

static unsigned long long nRx, nTx;

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
	printf("Received packet on device %s. Received by process %d. Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",
		getItemString(SLC_DATA_MODEL,tupel,"net.device"),
		getItemInt(SLC_DATA_MODEL,tupel,"process.process"),
		getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),
		tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	writeSample(tupel, timeUS);

	nRx++;

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
	printf("Transmitted packet on device %s. Sent by process %d. Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",
		getItemString(SLC_DATA_MODEL,tupel,"net.device"),
		getItemInt(SLC_DATA_MODEL,tupel,"process.process"),
		getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),
		tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	writeSample(tupel, timeUS);

	nTx++;

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
	char *outputFname = OUTPUT_FILENAME;

	optind = 1;
	opterr = 0;
	while ((opt = getopt(argc, argv, "d:ef:")) != -1) {
		switch (opt) {
		case 'd':
			devName = strdup(optarg);
			if (devName == NULL) {
				ERR_MSG("Cannot allocate memory for devName\n");
				return -1;
			}
			break;
		case 'f':
			outputFname = optarg;
			break;
		case 'e':
			useEvalReader = 1;
			break;
		default:
			ERR_MSG("Usage: %s [-d <device>] [-e] [-f outputFname]\n", __FILE__);
			return -1;
		}
	}
	if (devName == NULL) {
		devName = strdup(DEFAULT_DEVICE);
	}
	INFO_MSG("Monitoring device %s. %s timestamps (ofile: %s).\n", devName, (useEvalReader ? "Recording" : "Do not record"), outputFname);

	setupQueries();

	if (setupEvalWriter(outputFname) != 0) {
		return -1;
	}

	ret = registerQuery(&queryRXJoin);
	if (ret < 0 ) {
		ERR_MSG("Register eval tx/rx + join failed: %d\n",-ret);
		destroyEvalWriter();
		return -1;
	}
	INFO_MSG("Registered eval tx/rx + join queries\n");

	return 0;
}

int onUnload(void) {
	int ret = 0;

	ret = unregisterQuery(&queryRXJoin);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval tx/rx + join failed: %d\n",-ret);
		return -1;
	}

	destroyEvalWriter();

	INFO_MSG("nRx=%llu, nTx=%llu\n",nRx,nTx);
	free(devName);
	freeOperator(GET_BASE(rxStreamJoin),0);
	freeOperator(GET_BASE(txStreamJoin),0);
	INFO_MSG("Unregistered eval tx/rx + join queries\n");

	return 0;
}
