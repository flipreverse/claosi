#include <linux/module.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

static ObjectStream_t processObjCreate, processObjExit, processObjStatus, processObjStatusJoin;
static SourceStream_t processUTimeStr, processCommStr;
static Join_t joinComm, joinStime;
static Predicate_t commPredicate, stimePredicate, filterPIDPredicate;
static Filter_t filterPID;
static Query_t queryFork, queryExit, queryStatus, queryUTime, queryComm, queryJoin;

static void printResultFork(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p (processing duration: %llu us): task %d created\n",tupel->itemLen,tupel,timeUS - tupel->timestamp, getItemInt(SLC_DATA_MODEL,tupel,"process.process"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultExit(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p (processing duration: %llu us): task %d terminated\n",tupel->itemLen,tupel,timeUS - tupel->timestamp, getItemInt(SLC_DATA_MODEL,tupel,"process.process"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultStatus(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p (processing duration: %llu us): task %d status\n",tupel->itemLen,tupel,timeUS - tupel->timestamp, getItemInt(SLC_DATA_MODEL,tupel,"process.process"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultUTime(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p (process duration: %llu us): task utime %d\n",tupel->itemLen,tupel,timeUS - tupel->timestamp, getItemInt(SLC_DATA_MODEL,tupel,"process.process.utime"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultComm(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p (process duration: %llu us): task comm %s\n",tupel->itemLen,tupel,timeUS - tupel->timestamp, getItemString(SLC_DATA_MODEL,tupel,"process.process.comm"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultJoin(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p (process duration: %llu us): task %d: comm %s, stime %d\n",
					tupel->itemLen,
					tupel,
					timeUS - tupel->timestamp,
					getItemInt(SLC_DATA_MODEL,tupel,"process.process"),
					getItemString(SLC_DATA_MODEL,tupel,"process.process.comm"),
					getItemInt(SLC_DATA_MODEL,tupel,"process.process.stime"));
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryFork);
	queryFork.next = &queryExit;
	queryFork.onQueryCompleted = printResultFork;
	queryFork.root = GET_BASE(processObjCreate);
	INIT_OBJ_STREAM(processObjCreate,"process.process",0,0,NULL,OBJECT_CREATE);

	initQuery(&queryExit);
	queryExit.next = &queryStatus;
	queryExit.onQueryCompleted = printResultExit;
	queryExit.root = GET_BASE(processObjExit);
	INIT_OBJ_STREAM(processObjExit,"process.process",0,0,NULL,OBJECT_DELETE);

	initQuery(&queryStatus);
	queryStatus.next = &queryUTime;
	queryStatus.onQueryCompleted = printResultStatus;
	queryStatus.root = GET_BASE(processObjStatus);
	INIT_OBJ_STREAM(processObjStatus,"process.process",0,0,NULL,OBJECT_STATUS);

	initQuery(&queryUTime);
	queryUTime.next = &queryComm;
	queryUTime.onQueryCompleted = printResultUTime;
	queryUTime.root = GET_BASE(processUTimeStr);
	INIT_SRC_STREAM(processUTimeStr,"process.process.utime",1,0,NULL,2000);
	SET_SELECTOR_INT(processUTimeStr,0,3568)

	initQuery(&queryComm);
	queryComm.onQueryCompleted = printResultComm;
	queryComm.root = GET_BASE(processCommStr);
	INIT_SRC_STREAM(processCommStr,"process.process.comm",1,0,NULL,2000);
	SET_SELECTOR_INT(processCommStr,0,3568)

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
}

int __init processqueries_init(void)
{
	int ret = 0;
	setupQueries();

	ret = registerQuery(&queryJoin);
	if (ret < 0 ) {
		ERR_MSG("Register queries failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Sucessfully registered process queries\n");

	return 0;
}

void __exit processqueries_exit(void) {
	int ret = 0;

	ret = unregisterQuery(&queryJoin);
	if (ret < 0 ) {
		ERR_MSG("Unregister queries failed: %d\n",-ret);
	}

	freeOperator(GET_BASE(processObjCreate),0);
	freeOperator(GET_BASE(processObjExit),0);
	freeOperator(GET_BASE(processObjStatus),0);
	freeOperator(GET_BASE(processUTimeStr),0);
	freeOperator(GET_BASE(processCommStr),0);
	freeOperator(GET_BASE(processObjStatusJoin),0);
	DEBUG_MSG(1,"Unregistered process queries\n");
}

module_init(processqueries_init);
module_exit(processqueries_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
