#include <linux/module.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

#define PRINT_TUPLE
#undef PRINT_TUPLE

static EventStream_t rxStream, txStream, txStream2;
static SourceStream_t rxBytesSrc, txBytesSrc;
static ObjectStream_t devStatusStream, devObjStream;
static Predicate_t filterRXPredicate, rxBytesPredicate;
static Join_t joinRXBytes;
static Filter_t filter;
static Query_t queryRX, queryTX,queryTX2, queryRXBytes, queryTXBytes, queryDevStatus, queryDevObj;

static void printResultRx(unsigned int id, Tupel_t *tupel) {
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
#ifdef PRINT_TUPLE
	printk("Received packet on device %s. Device received %d bytes so far. (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"net.device.rxBytes"),tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

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
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif
#ifdef PRINT_TUPLE
	printk("Received tupel with %d items at memory address %p at %llu us, dev=%s, tx\n",tupel->itemLen,tupel,timeUS - tupel->timestamp,getItemString(SLC_DATA_MODEL,tupel,"net.device"));
#endif

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultRxBytes(unsigned int id, Tupel_t *tupel) {
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
#ifdef PRINT_TUPLE
	printk("Received tupel with %d items at memory address %p at %llu us, dev=%s, rxBytes=%d\n",tupel->itemLen,tupel,timeUS - tupel->timestamp,getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"net.device.rxBytes"));
#endif

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTxBytes(unsigned int id, Tupel_t *tupel) {
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
#ifdef PRINT_TUPLE
	printk("Received tupel with %d items at memory address %p at %llu us, dev=%s, txBytes=%d\n",tupel->itemLen,tupel,timeUS - tupel->timestamp,getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"net.device.txBytes"));
#endif

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultDevStatus(unsigned int id, Tupel_t *tupel) {
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
#ifdef PRINT_TUPLE
	printk("Received tupel with %d items at memory address %p at %llu us, dev=%s status\n",tupel->itemLen,tupel,timeUS - tupel->timestamp,getItemString(SLC_DATA_MODEL,tupel,"net.device"));
#endif

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultDev(unsigned int id, Tupel_t *tupel) {
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
#ifdef PRINT_TUPLE
	printk("Received tupel with %d items at memory address %p at %llu us, dev=%s changed\n",tupel->itemLen,tupel,timeUS - tupel->timestamp,getItemString(SLC_DATA_MODEL,tupel,"net.device"));
#endif

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryRX);
	queryRX.onQueryCompleted = printResultRx;
	queryRX.root = GET_BASE(rxStream);
	//queryRX.next = &queryRXBytes;
	INIT_EVT_STREAM(rxStream,"net.device.onRx",1,0,GET_BASE(joinRXBytes))
	SET_SELECTOR_STRING(rxStream,0,"eth1")
	INIT_FILTER(filter,GET_BASE(joinRXBytes),1)
	ADD_PREDICATE(filter,0,filterRXPredicate)
	SET_PREDICATE(filterRXPredicate,EQUAL, OP_STREAM, "net.packetType.macProtocol", OP_POD, "42")
	INIT_JOIN(joinRXBytes,"net.device.rxBytes",NULL,1)
	ADD_PREDICATE(joinRXBytes,0,rxBytesPredicate)
	SET_PREDICATE(rxBytesPredicate,EQUAL, OP_STREAM, "net.device", OP_JOIN, "net.device")

	initQuery(&queryRXBytes);
	queryRXBytes.onQueryCompleted = printResultRxBytes;
	queryRXBytes.root = GET_BASE(rxBytesSrc);
	//queryRXBytes.next = &queryTX;
	INIT_SRC_STREAM(rxBytesSrc,"net.device.rxBytes",1,0,NULL,2000)
	SET_SELECTOR_STRING(rxBytesSrc,0,"eth1")

	initQuery(&queryTX);
	queryTX.onQueryCompleted = printResultTx;
	queryTX.root = GET_BASE(txStream);
	//queryTX.next = &queryTX2;
	INIT_EVT_STREAM(txStream,"net.device.onTx",1,0,NULL)
	SET_SELECTOR_STRING(txStream,0,"eth1")

	initQuery(&queryTX2);
	queryTX2.onQueryCompleted = printResultTx;
	queryTX2.root = GET_BASE(txStream2);
	//queryTX2.next = &queryTXBytes;
	INIT_EVT_STREAM(txStream2,"net.device.onTx",1,0,NULL)
	SET_SELECTOR_STRING(txStream2,0,"eth0")

	initQuery(&queryTXBytes);
	queryTXBytes.onQueryCompleted = printResultTxBytes;
	queryTXBytes.root = GET_BASE(txBytesSrc);
	//queryTXBytes.next = &queryDevStatus;
	INIT_SRC_STREAM(txBytesSrc,"net.device.txBytes",1,0,NULL,2000)
	SET_SELECTOR_STRING(txBytesSrc,0,"eth0")
	
	initQuery(&queryDevStatus);
	queryDevStatus.onQueryCompleted = printResultDevStatus;
	queryDevStatus.root = GET_BASE(devStatusStream);
	//queryDevStatus.next = &queryDevObj;
	INIT_OBJ_STREAM(devStatusStream,"net.device",0,0,NULL,OBJECT_STATUS);

	initQuery(&queryDevObj);
	queryDevObj.onQueryCompleted = printResultDev;
	queryDevObj.root = GET_BASE(devObjStream);
	INIT_OBJ_STREAM(devObjStream,"net.device",0,0,NULL,OBJECT_CREATE|OBJECT_DELETE);
}

int __init netqueries_init(void)
{
	int ret = 0;
	setupQueries();

	if ((ret = registerQuery(&queryRX)) < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Registered net queries\n");

	return 0;
}

void __exit netqueries_exit(void) {
	int ret = 0;

	ret = unregisterQuery(&queryRX);
	if (ret < 0 ) {
		ERR_MSG("Unregister failed: %d\n",-ret);
	}

	freeOperator(GET_BASE(rxStream),0);
	freeOperator(GET_BASE(rxBytesSrc),0);
	freeOperator(GET_BASE(txStream),0);
	freeOperator(GET_BASE(txStream2),0);
	freeOperator(GET_BASE(txBytesSrc),0);
	freeOperator(GET_BASE(devStatusStream),0);
	freeOperator(GET_BASE(devObjStream),0);
	DEBUG_MSG(1,"Unregistered net queries\n");
}

module_init(netqueries_init);
module_exit(netqueries_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
