#include <query.h>
#include <api.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

static Query_t queryUtime;
static SourceStream_t utimeStream;
static Predicate_t utimePred;
static Filter_t utimeFilter;

static void printResult(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printf("processing duration: %llu us, query id: %u,",timeUS - tupel->timestamp,id);
	printTupel(SLC_DATA_MODEL,tupel);
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryUtime);
	queryUtime.onQueryCompleted = printResult;
	queryUtime.root = GET_BASE(utimeStream);
	INIT_SRC_STREAM(utimeStream,"process.process.utime",0,GET_BASE(utimeFilter),2000)
	INIT_FILTER(utimeFilter,NULL,1)
	ADD_PREDICATE(utimeFilter,0,utimePred)
	SET_PREDICATE(utimePred,GEQ, STREAM, "process.process.utime", POD, "4710")
}

int onLoad(void) {
	int ret = 0;

	setupQueries();

	if ((ret = registerQuery(&queryUtime)) < 0 ) {
		ERR_MSG("Query registration failed: %d\n",-ret);
		return -1;
	}

	return 0;
}

int onUnload(void) {
	int ret = 0;

	if ((ret = unregisterQuery(&queryUtime)) < 0 ) {
		ERR_MSG("Unregister queries failed: %d\n",-ret);
	}

	freeOperator(GET_BASE(utimeStream),0);

	return 0;
}
