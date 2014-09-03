#include <query.h>
#include <api.h>
#include <statistic.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define PRINT_TUPLE
#undef PRINT_TUPLE

static Query_t queryJoin, queryRX;
static ObjectStream_t processObjStatusJoin;
static EventStream_t rxStream;
static Join_t joinComm, joinStime, joinRXBytes;
static Predicate_t commPredicate, stimePredicate, filterPIDPredicate, rxBytesPredicate;
static Filter_t filterPID;

DECLARE_STATISTIC_VARS(Fork);
DECLARE_STATISTIC_VARS(RX);

static void printResultJoin(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS, duration = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif
	duration = timeUS - tupel->timestamp;

#ifdef PRINT_TUPLE
	printf("Received tupel with %d items at memory address %p (process duration: %llu us): task %d: comm %s, stime %d\n",
					tupel->itemLen,
					tupel,
					duration,
					getItemInt(SLC_DATA_MODEL,tupel,"process.process"),
					getItemString(SLC_DATA_MODEL,tupel,"process.process.comm"),
					getItemInt(SLC_DATA_MODEL,tupel,"process.process.stime"));
#endif
	ACCOUNT_STATISTIC(Fork,duration);

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultRx(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0, duration = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif
	duration = timeUS - tupel->timestamp;

#ifdef PRINT_TUPLE
	printf("Received packet on device %s. Device received %d bytes so far. (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"net.device.rxBytes"),tupel->itemLen,tupel,duration);
#endif
	ACCOUNT_STATISTIC(RX,duration);

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryJoin);
	queryJoin.onQueryCompleted = printResultJoin;
	queryJoin.root = GET_BASE(processObjStatusJoin);
	//queryJoin.next = &queryRX;
	INIT_OBJ_STREAM(processObjStatusJoin,"process.process",0,0,GET_BASE(filterPID),OBJECT_CREATE);
	INIT_FILTER(filterPID,GET_BASE(joinStime),1)
	ADD_PREDICATE(filterPID,0,filterPIDPredicate)
	SET_PREDICATE(filterPIDPredicate,GEQ, OP_STREAM, "process.process", OP_POD, "3000")
	INIT_JOIN(joinStime,"process.process.stime", GET_BASE(joinComm),1)
	ADD_PREDICATE(joinStime,0,stimePredicate)
	SET_PREDICATE(stimePredicate,EQUAL, OP_STREAM, "process.process", OP_JOIN, "process.process")
	INIT_JOIN(joinComm,"process.process.comm", NULL,1)
	ADD_PREDICATE(joinComm,0,commPredicate)
	SET_PREDICATE(commPredicate,EQUAL, OP_STREAM, "process.process", OP_JOIN, "process.process")

	initQuery(&queryRX);
	queryRX.onQueryCompleted = printResultRx;
	queryRX.root = GET_BASE(rxStream);
	INIT_EVT_STREAM(rxStream,"net.device.onRx",1,0,GET_BASE(joinRXBytes));
	SET_SELECTOR_STRING(rxStream,0,"eth1");
	INIT_JOIN(joinRXBytes,"net.device.rxBytes",NULL,1)
	ADD_PREDICATE(joinRXBytes,0,rxBytesPredicate)
	SET_PREDICATE(rxBytesPredicate,EQUAL, OP_STREAM, "net.device", OP_JOIN, "net.device")
}

int onLoad(void) {
	int ret = 0;

	setupQueries();

	if ((ret = registerQuery(&queryJoin)) < 0 ) {
		ERR_MSG("Query registration failed: %d\n",-ret);
		return -1;
	}

	return 0;
}

int onUnload(void) {
	int ret = 0;

	if ((ret = unregisterQuery(&queryJoin)) < 0 ) {
		ERR_MSG("Unregister queries failed: %d\n",-ret);
	}

	printf("fork: n = %lu, xn = %e, sn = %e\n",nFork,xnFork,snFork);
	printf("rx: n = %lu, xn = %e, sn = %e\n",nRX,xnRX,snRX);

	freeOperator(GET_BASE(processObjStatusJoin),0);
	freeOperator(GET_BASE(rxStream),0);

	return 0;
}
