#define MSG_FMT(fmt) "[slc-eval0] " fmt
#include <linux/module.h>
#include <linux/relay.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>
#include <evaluation.h>

static ObjectStream_t processObjFork;
static Join_t joinComm, joinStime;
static Predicate_t stimePredicate, commPredicate;
static Query_t queryForkJoin;

static int useRelayFS = 0;
module_param(useRelayFS, int, S_IRUGO);

#include "eval-relay.c"

static void printResultForkJoin(unsigned int id, Tupel_t *tupel) {
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

	sample.ts1 = tupel->timestamp;
#ifdef EVALUATION
	sample.ts2 = tupel->timestamp2;
	sample.ts3 = tupel->timestamp3;
#endif
	sample.ts4 = timeUS;

	if (useRelayFS) {
		relay_write(relayfsOutput,&sample,sizeof(sample));
	}

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void initQueriesFork(void) {
	initQuery(&queryForkJoin);
	queryForkJoin.onQueryCompleted = printResultForkJoin;
	queryForkJoin.root = GET_BASE(processObjFork);
	//queryForkJoin.next = & querySockets;
	INIT_OBJ_STREAM(processObjFork,"process.process",0,0,GET_BASE(joinStime),OBJECT_CREATE);
	INIT_JOIN(joinStime,"process.process.stime", GET_BASE(joinComm),1)
	ADD_PREDICATE(joinStime,0,stimePredicate)
	SET_PREDICATE(stimePredicate,EQUAL, OP_STREAM, "process.process", OP_JOIN, "process.process")
	INIT_JOIN(joinComm,"process.process.comm", NULL,1)
	ADD_PREDICATE(joinComm,0,commPredicate)
	SET_PREDICATE(commPredicate,EQUAL, OP_STREAM, "process.process", OP_JOIN, "process.process")
}

int __init evalqueries_0_init(void) {
	int ret = 0;

	initQueriesFork();


	if (useRelayFS) {
		if (initRelayFS() < 0) {
			return -1;
		}
	}

	ret = registerQuery(&queryForkJoin);
	if (ret < 0 ) {
		ERR_MSG("Register eval fork failed: %d\n",-ret);
		destroyRelayFS();
		return -1;
	}
	INFO_MSG("Registered eval fork queries\n");

	return 0;
}

void __exit evalqueries_0_exit(void) {
	int ret = 0;

	ret = unregisterQuery(&queryForkJoin);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval fork failed: %d\n",-ret);
	}

	if (useRelayFS) {
		destroyRelayFS();
	}

	freeOperator(GET_BASE(processObjFork),0);

	INFO_MSG("Unregistered eval fork queries\n");
}

module_init(evalqueries_0_init);
module_exit(evalqueries_0_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
