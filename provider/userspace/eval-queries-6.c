#define MSG_FMT(fmt) "[slc-eval6] " fmt
#include <datamodel.h>
#include <query.h>
#include <api.h>
#include <evalwriter.h>
#include <sys/time.h>

#define PRINT_TUPLE
#undef PRINT_TUPLE

#define DEFAULT_DEVICE "eth1"

#define OUTPUT_FILENAME "delay-slc-6.txt"

static EventStream_t rxStreamBase, txStreamBase;
static Select_t rxSelectPacket, txSelectPacket;
static Filter_t rxFilterData, txFilterData;
static Predicate_t rxPredData, txPredData;
static Element_t rxElemPacket, txElemPacket;
static Query_t queryRXBase, queryTXBase;
static char *devName = NULL;
static unsigned long long nRx, nTx;

static void printResultRx(unsigned int id, Tupel_t *tupel) {
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
	printf("Received packet. Mac protocol = %hhd, Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",
		getItemByte(SLC_DATA_MODEL,tupel,"net.packetType.macProtocol"),
		getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),
		tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	writeSample(tupel, timeUS);

	nRx++;

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTx(unsigned int id, Tupel_t *tupel) {
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
	printf("Transmitted packet. Mac protocol = %hhd, Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",
		getItemByte(SLC_DATA_MODEL,tupel,"net.packetType.macProtocol"),
		getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),
		tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	writeSample(tupel, timeUS);

	nTx++;

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryRXBase);
	queryRXBase.onQueryCompleted = printResultRx;
	queryRXBase.root = GET_BASE(rxStreamBase);
	queryRXBase.next = &queryTXBase;
	INIT_EVT_STREAM(rxStreamBase,"net.device.onRx",1,0,GET_BASE(rxFilterData))
	SET_SELECTOR_STRING(rxStreamBase,0,devName)
	INIT_FILTER(rxFilterData,GET_BASE(rxSelectPacket),1)
	ADD_PREDICATE(rxFilterData,0,rxPredData)
	SET_PREDICATE(rxPredData,GEQ, OP_STREAM, "net.packetType.dataLength", OP_POD, "1000")
	INIT_SELECT(rxSelectPacket,NULL,1)
	ADD_ELEMENT(rxSelectPacket,0,rxElemPacket,"net.packetType")

	initQuery(&queryTXBase);
	queryTXBase.onQueryCompleted = printResultTx;
	queryTXBase.root = GET_BASE(txStreamBase);
	INIT_EVT_STREAM(txStreamBase,"net.device.onTx",1,0,GET_BASE(txFilterData))
	SET_SELECTOR_STRING(txStreamBase,0,devName)
	INIT_FILTER(txFilterData,GET_BASE(txSelectPacket),1)
	ADD_PREDICATE(txFilterData,0,txPredData)
	SET_PREDICATE(txPredData,GEQ, OP_STREAM, "net.packetType.dataLength", OP_POD, "10")
	INIT_SELECT(txSelectPacket,NULL,1)
	ADD_ELEMENT(txSelectPacket,0,txElemPacket,"net.packetType")
}

int onLoad(int argc, char *argv[]) {
	int ret = 1, opt;
	char *outputFname = OUTPUT_FILENAME;

	optind = 1;
	opterr = 0;
	while ((opt = getopt(argc, argv, "d:ef:")) != -1) {
		switch (opt) {
		case 'd':
			devName = strdup(optarg);
			if (devName == NULL) {
				ERR_MSG("Cannot allocate memory for devName (getopt())\n");
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
		if (devName == NULL) {
			ERR_MSG("Cannot allocate memory for devName (DEFAULT)\n");
			return -1;
		}
	}
	INFO_MSG("Monitoring device %s. %s timestamps (ofile: %s).\n", devName, (useEvalReader ? "Recording" : "Do not record"), outputFname);

	setupQueries();

	if (setupEvalWriter(outputFname) != 0) {
		ERR_MSG("Cannot setup evalwriter\n");
		return -1;
	}

	ret = registerQuery(&queryRXBase);
	if (ret < 0 ) {
		ERR_MSG("Register eval tx/rx + join failed: %d\n",-ret);
		destroyEvalWriter();
	}
	INFO_MSG("Registered eval tx/rx queries\n");

	return 0;
}

int onUnload(void) {
	int ret = 0;

	ret = unregisterQuery(&queryRXBase);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval tx/rx failed: %d\n",-ret);
		return -1;
	}

	destroyEvalWriter();

	INFO_MSG("nRx=%llu, nTx=%llu\n",nRx,nTx);
	free(devName);
	freeOperator(GET_BASE(rxStreamBase),0);
	freeOperator(GET_BASE(txStreamBase),0);
	INFO_MSG("Unregistered eval tx/rx queries\n");

	return 0;
}
