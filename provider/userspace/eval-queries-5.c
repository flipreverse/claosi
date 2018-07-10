#define MSG_FMT(fmt) "[slc-eval5] " fmt
#include <datamodel.h>
#include <query.h>
#include <api.h>
#include <evalwriter.h>
#include <sys/time.h>

#define PRINT_TUPLE
#undef PRINT_TUPLE

#define OUTPUT_FILENAME "delay-slc-5.txt"

static ObjectStream_t processObjFork;
static Join_t joinComm, joinStime;
static Predicate_t stimePredicate, commPredicate;
static Query_t queryForkJoin;

static void printResultForkJoin(unsigned int id, Tupel_t *tupel) {
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

	writeSample(tupel, timeUS);

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
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

int onLoad(int argc, char *argv[]) {
	int ret = 0, opt;
	char *outputFname = OUTPUT_FILENAME;

	optind = 1;
	opterr = 0;
	while ((opt = getopt(argc, argv, "ef:")) != -1) {
		switch (opt) {
		case 'e':
			useEvalReader = 1;
			break;
		case 'f':
			outputFname = optarg;
			break;
		default:
			ERR_MSG("Usage: %s [-e] [-f outputFname]\n", __FILE__);
			return -1;
		}
	}
	INFO_MSG("%s timestamps (ofile: %s).\n", (useEvalReader ? "Recording" : "Do not record"), outputFname)

	setupQueries();

	setupEvalWriter(outputFname);

	ret = registerQuery(&queryForkJoin);
	if (ret < 0 ) {
		ERR_MSG("Register eval fork failed: %d\n",-ret);
		destroyEvalWriter();
		return -1;
	}
	INFO_MSG("Registered eval fork queries\n");

	return 0;
}

int onUnload(void) {
	int ret = 0;

	ret = unregisterQuery(&queryForkJoin);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval fork failed: %d\n",-ret);
		return -1;
	}

	destroyEvalWriter();

	freeOperator(GET_BASE(processObjFork),0);
	INFO_MSG("Unregistered eval fork queries\n");

	return 0;
}
