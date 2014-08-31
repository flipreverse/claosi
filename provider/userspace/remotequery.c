#include <query.h>
#include <api.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

static Query_t queryUtime, queryJoin, queryRX;
static SourceStream_t utimeStream;
static ObjectStream_t processObjStatusJoin;
static EventStream_t rxStream;
static Join_t joinComm, joinStime, joinRXBytes;
static Predicate_t commPredicate, stimePredicate, filterPIDPredicate, utimePred, rxBytesPredicate;
static Filter_t filterPID, utimeFilter;

static void printResult(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif
	printf("processing duration: %llu us, query id: %u,",timeUS - tupel->timestamp,id);
	printTupel(SLC_DATA_MODEL,tupel);
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultJoin(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif
	printf("Received tupel with %d items at memory address %p (process duration: %llu us): task %d: comm %s, stime %d\n",
					tupel->itemLen,
					tupel,
					timeUS - tupel->timestamp,
					getItemInt(SLC_DATA_MODEL,tupel,"process.process"),
					getItemString(SLC_DATA_MODEL,tupel,"process.process.comm"),
					getItemInt(SLC_DATA_MODEL,tupel,"process.process.stime"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

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
	printf("Received packet on device %s. Device received %d bytes so far. (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"net.device.rxBytes"),tupel->itemLen,tupel,timeUS - tupel->timestamp);
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryUtime);
	queryUtime.onQueryCompleted = printResult;
	queryUtime.root = GET_BASE(utimeStream);
	INIT_SRC_STREAM(utimeStream,"process.process.utime",0,0,GET_BASE(utimeFilter),2000)
	INIT_FILTER(utimeFilter,NULL,1)
	ADD_PREDICATE(utimeFilter,0,utimePred)
	SET_PREDICATE(utimePred,GEQ, OP_STREAM, "process.process.utime", OP_POD, "4710")

	initQuery(&queryJoin);
	queryJoin.onQueryCompleted = printResultJoin;
	queryJoin.root = GET_BASE(processObjStatusJoin);
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

	if ((ret = registerQuery(&queryRX)) < 0 ) {
		ERR_MSG("Query registration failed: %d\n",-ret);
		return -1;
	}

	return 0;
}

int onUnload(void) {
	int ret = 0;

	if ((ret = unregisterQuery(&queryRX)) < 0 ) {
		ERR_MSG("Unregister queries failed: %d\n",-ret);
	}

	freeOperator(GET_BASE(utimeStream),0);
	freeOperator(GET_BASE(processObjStatusJoin),0);
	freeOperator(GET_BASE(rxStream),0);

	return 0;
}
