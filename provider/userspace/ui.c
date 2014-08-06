#include <query.h>
#include <api.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define REGISTER_QUERIES
//#undef REGISTER_QUERIES

DECLARE_ELEMENTS(nsUI, model)
DECLARE_ELEMENTS(evtDisplay, typeEventType, srcForegroundApp, objApp)
DECLARE_ELEMENTS(typeXPos, typeYPos)

#ifdef REGISTER_QUERIES
static Query_t queryDisplay, queryApp, queryForeground;
static EventStream_t displayStream;
static ObjectStream_t appStream;
static SourceStream_t fappStream;
static Predicate_t xPosPred, yPosPred;
static Filter_t posFilter;
#endif

static pthread_t displayEvtThread;
static pthread_attr_t displayEvtThreadAttr;
static int displayEvtThreadRunning = 0;

static void* displayEvtWork(void *data) {
	Tupel_t *tuple = NULL;
	struct timeval curTime;
	unsigned long long timeMS = 0;

	while (1) {
		sleep(3);
		if (displayEvtThreadRunning == 0) {
			break;
		}
		gettimeofday(&curTime,NULL);
		timeMS = (long long)curTime.tv_sec * (long long)USEC_PER_MSEC + (long long)curTime.tv_usec;
		tuple = initTupel(timeMS,1);
		if (tuple == NULL) {
			continue;
		}

		srand(time(0));
		ACQUIRE_READ_LOCK(slcLock);
		allocItem(SLC_DATA_MODEL,tuple,0,"ui.eventType");
		setItemInt(SLC_DATA_MODEL,tuple,"ui.eventType.xPos",rand() % 1024);
		setItemInt(SLC_DATA_MODEL,tuple,"ui.eventType.yPos",rand() % 1024);
		RELEASE_READ_LOCK(slcLock);
		eventOccured("ui.display",tuple);
	}

	pthread_exit(0);
	return NULL;
}

static void activateDisplay(void) {

	displayEvtThreadRunning = 1;
	pthread_attr_init(&displayEvtThreadAttr);
	pthread_attr_setdetachstate(&displayEvtThreadAttr, PTHREAD_CREATE_JOINABLE);
	if (pthread_create(&displayEvtThread,&displayEvtThreadAttr,displayEvtWork,NULL) < 0) {
		perror("pthread_create displayEvtThread");
	}

}

static void deactivateDisplay(void) {
	displayEvtThreadRunning = 0;
	pthread_join(displayEvtThread,NULL);
	pthread_attr_destroy(&displayEvtThreadAttr);
}

static void activateApp(void) {
	
}

static void deactivateApp(void) {
	
}

static Tupel_t* statusApp(void) {
	Tupel_t *tuple = NULL;
	struct timeval curTime;
	char *name = NULL;
	unsigned long long timeMS = 0;

	gettimeofday(&curTime,NULL);
	timeMS = (long long)curTime.tv_sec * (long long)USEC_PER_MSEC + (long long)curTime.tv_usec;
	tuple = initTupel(timeMS,1);
	if (tuple == NULL) {
		return NULL;
	}

	name = ALLOC(strlen("lol.app") + 1);
	if (name == NULL) {
		return NULL;
	}
	strcpy(name,"lol.app");
	allocItem(SLC_DATA_MODEL,tuple,0,"ui.app");
	setItemString(SLC_DATA_MODEL,tuple,"ui.app",name);

	return tuple;
}

static Tupel_t* sourceForegroundApp(void) {
	Tupel_t *tuple = NULL;
	struct timeval curTime;
	char *name = NULL;
	unsigned long long timeMS = 0;

	gettimeofday(&curTime,NULL);
	timeMS = (long long)curTime.tv_sec * (long long)USEC_PER_MSEC + (long long)curTime.tv_usec;
	tuple = initTupel(timeMS,1);
	if (tuple == NULL) {
		return NULL;
	}

	name = ALLOC(strlen("pferd.app") + 1);
	if (name == NULL) {
		return NULL;
	}
	strcpy(name,"pferd.app");
	allocItem(SLC_DATA_MODEL,tuple,0,"ui.foregroundApp");
	setItemString(SLC_DATA_MODEL,tuple,"ui.foregroundApp",name);

	return tuple;
}

static void initDatamodel(void) {
	int i = 0;

	INIT_EVENT_COMPLEX(evtDisplay,"display",nsUI,"ui.eventType",activateDisplay,deactivateDisplay)
	//INIT_SOURCE_COMPLEX(srcProcessess,"processes",objApp,"process.process",getSrc) //TODO: should be an array as well
	
	INIT_OBJECT(objApp,"app",nsUI,0,STRING,activateApp,deactivateApp,statusApp)
	//ADD_CHILD(objApp,0,srcProcessess)

	INIT_SOURCE_POD(srcForegroundApp,"foregroundApp",nsUI,STRING,sourceForegroundApp)
	
	INIT_PLAINTYPE(typeXPos,"xPos",typeEventType,INT)
	INIT_PLAINTYPE(typeYPos,"yPos",typeEventType,INT)
	INIT_TYPE(typeEventType,"eventType",nsUI,2)
	ADD_CHILD(typeEventType,0,typeXPos)
	ADD_CHILD(typeEventType,1,typeYPos)

	INIT_NS(nsUI,"ui",model,4)
	ADD_CHILD(nsUI,0,evtDisplay)
	ADD_CHILD(nsUI,1,typeEventType)
	ADD_CHILD(nsUI,2,srcForegroundApp)
	ADD_CHILD(nsUI,3,objApp)

	INIT_MODEL(model,1)
	ADD_CHILD(model,0,nsUI)
}

#ifdef REGISTER_QUERIES
static void printResult(QueryID_t id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeMS;

	gettimeofday(&time,NULL);
	timeMS = (long long)time.tv_sec * (long long)USEC_PER_MSEC + (long long)time.tv_usec;
	printf("processing duration: %llu ms,",timeMS - tupel->timestamp);
	printTupel(SLC_DATA_MODEL,tupel);
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void initQuery(void) {
	queryDisplay.next = &queryApp;
	queryDisplay.queryType = ASYNC;
	queryDisplay.queryID = 0;
	queryDisplay.onQueryCompleted = printResult;
	queryDisplay.root = GET_BASE(displayStream);
	INIT_EVT_STREAM(displayStream,"ui.display",0,GET_BASE(posFilter))
	INIT_FILTER(posFilter,NULL,2)
	ADD_PREDICATE(posFilter,0,xPosPred)
	SET_PREDICATE(xPosPred,GEQ, STREAM, "ui.eventType.xPos", POD, "700")
	ADD_PREDICATE(posFilter,1,yPosPred)
	SET_PREDICATE(yPosPred,LEQ, STREAM, "ui.eventType.yPos", POD, "300")

	queryApp.next = &queryForeground;
	queryApp.queryType = ASYNC;
	queryApp.queryID = 0;
	queryApp.onQueryCompleted = printResult;
	queryApp.root = GET_BASE(appStream);
	INIT_OBJ_STREAM(appStream,"ui.app",0,NULL,OBJECT_STATUS)

	queryForeground.next = NULL;
	queryForeground.queryType = ASYNC;
	queryForeground.queryID = 0;
	queryForeground.onQueryCompleted = printResult;
	queryForeground.root = GET_BASE(fappStream);
	INIT_SRC_STREAM(fappStream,"ui.foregroundApp",0,NULL,1000)
}
#endif

int onLoad(void) {
	int ret = 0;

	initDatamodel();
#ifdef REGISTER_QUERIES
	initQuery();
#endif

	if ((ret = registerProvider(&model, NULL)) < 0 ) {
		ERR_MSG("Register provider ui failed: %d\n",-ret);
		return -1;
	}
#ifdef REGISTER_QUERIES
	if ((ret = registerQuery(&queryDisplay)) < 0 ) {
		unregisterProvider(&model, NULL);
		ERR_MSG("Query registration failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Sucessfully registered datamodel for ui and queries.\n");
#endif
	DEBUG_MSG(1,"Registered ui provider\n");
	
	
	return 0;
}

int onUnload(void) {
	int ret = 0;

#ifdef REGISTER_QUERIES
	if ((ret = unregisterQuery(&queryDisplay)) < 0 ) {
		ERR_MSG("Unregister queries failed: %d\n",-ret);
	}
#endif
	if ((ret = unregisterProvider(&model, NULL)) < 0 ) {
		ERR_MSG("Unregister datamodel ui failed: %d\n",-ret);
	}

#ifdef REGISTER_QUERIES
	freeQuery(GET_BASE(displayStream),0);
	freeQuery(GET_BASE(appStream),0);
#endif
	freeDataModel(&model,0);

	DEBUG_MSG(1,"Unregistered ui provider\n");

	return 0;
}
