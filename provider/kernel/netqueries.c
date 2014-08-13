#include <linux/module.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

static EventStream_t rxStream, txStream;
static SourceStream_t rxBytesSrc;
static Predicate_t filterRXPredicate;
static Filter_t filter;
static Query_t queryRX, queryTX, queryRXBytes;


static void printResultRx(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeMS = 0;

	do_gettimeofday(&time);
	timeMS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p at %llu us, dev=%s, rx\n",tupel->itemLen,tupel,timeMS - tupel->timestamp,getItemString(SLC_DATA_MODEL,tupel,"net.device"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTx(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeMS = 0;

	do_gettimeofday(&time);
	timeMS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p at %llu us, dev=%s, tx\n",tupel->itemLen,tupel,timeMS - tupel->timestamp,getItemString(SLC_DATA_MODEL,tupel,"net.device"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultRxBytes(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeMS = 0;

	do_gettimeofday(&time);
	timeMS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p at %llu us, dev=%s, rxBytes=%d\n",tupel->itemLen,tupel,timeMS - tupel->timestamp,getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"net.device.rxBytes"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryRX);
	queryRX.onQueryCompleted = printResultRx;
	queryRX.root = GET_BASE(rxStream);
	queryRX.next = &queryRXBytes;
	INIT_EVT_STREAM(rxStream,"net.device.onRx",1,0,GET_BASE(filter))
	SET_SELECTOR_STRING_STREAM(rxStream,0,"eth1")
	INIT_FILTER(filter,NULL,1)
	ADD_PREDICATE(filter,0,filterRXPredicate)
	SET_PREDICATE(filterRXPredicate,EQUAL, STREAM, "net.packetType.macProtocol", POD, "42")

	initQuery(&queryRXBytes);
	queryRXBytes.onQueryCompleted = printResultRxBytes;
	queryRXBytes.root = GET_BASE(rxBytesSrc);
	queryRXBytes.next = &queryTX;
	INIT_SRC_STREAM(rxBytesSrc,"net.device.rxBytes",1,0,NULL,2000)
	SET_SELECTOR_STRING_STREAM(rxBytesSrc,0,"eth1")

	initQuery(&queryTX);
	queryTX.onQueryCompleted = printResultTx;
	queryTX.root = GET_BASE(txStream);
	INIT_EVT_STREAM(txStream,"net.device.onTx",1,0,NULL)
	SET_SELECTOR_STRING_STREAM(txStream,0,"eth1")
}

int __init netqueries_init(void)
{
	int ret = 0;
	setupQueries();

	if ((ret = registerQuery(&queryTX)) < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Registered net queries\n");

	return 0;
}

void __exit netqueries_exit(void) {
	int ret = 0;

	if ((ret = unregisterQuery(&queryTX)) < 0 ) {
		ERR_MSG("Unregister failed: %d\n",-ret);
	}

	freeOperator(GET_BASE(rxStream),0);
	freeOperator(GET_BASE(rxBytesSrc),0);
	freeOperator(GET_BASE(txStream),0);
	DEBUG_MSG(1,"Unregistered net queries\n");
}

module_init(netqueries_init);
module_exit(netqueries_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");