#include <query.h>
#include <api.h>
#include <sys/time.h>

static Query_t queryDisplay, queryApp, queryForeground;
static EventStream_t displayStream;
static ObjectStream_t appStream;
static SourceStream_t fappStream;
static Predicate_t xPosPred, yPosPred;
static Filter_t posFilter;

static void printResult(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printf("processing duration: %llu us,",timeUS - tupel->timestamp);
	printTupel(SLC_DATA_MODEL,tupel);
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryDisplay);
	queryDisplay.next = &queryApp;
	queryDisplay.onQueryCompleted = printResult;
	queryDisplay.root = GET_BASE(displayStream);
	INIT_EVT_STREAM(displayStream,"ui.display",0,0,GET_BASE(posFilter))
	INIT_FILTER(posFilter,NULL,2)
	ADD_PREDICATE(posFilter,0,xPosPred)
	SET_PREDICATE(xPosPred,GEQ, OP_STREAM, "ui.eventType.xPos", OP_POD, "700")
	ADD_PREDICATE(posFilter,1,yPosPred)
	SET_PREDICATE(yPosPred,LEQ, OP_STREAM, "ui.eventType.yPos", OP_POD, "300")

	initQuery(&queryApp);
	queryApp.next = &queryForeground;
	queryApp.onQueryCompleted = printResult;
	queryApp.root = GET_BASE(appStream);
	INIT_OBJ_STREAM(appStream,"ui.app",0,0,NULL,OBJECT_STATUS)

	initQuery(&queryForeground);
	queryForeground.onQueryCompleted = printResult;
	queryForeground.root = GET_BASE(fappStream);
	INIT_SRC_STREAM(fappStream,"ui.foregroundApp",0,0,NULL,1000)
}

int onLoad(void) {
	int ret = 0;

	setupQueries();

	ret = registerQuery(&queryDisplay);
	if (ret < 0 ) {
		ERR_MSG("Query registration failed: %d\n",-ret);
		return -1;
	}

	DEBUG_MSG(1,"Registered ui queries\n");
	return 0;
}

int onUnload(void) {
	int ret = 0;

	ret = unregisterQuery(&queryDisplay);
	if (ret < 0 ) {
		ERR_MSG("Unregister queries failed: %d\n",-ret);
	}
	freeOperator(GET_BASE(displayStream),0);
	freeOperator(GET_BASE(appStream),0);

	DEBUG_MSG(1,"Unregistered ui queries\n");
	return 0;
}

