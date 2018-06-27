#include <query.h>
#include <api.h>
#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>


DECLARE_ELEMENTS(nsUI, model)
DECLARE_ELEMENTS(evtDisplay, typeEventType, srcForegroundApp, objApp)
DECLARE_ELEMENTS(typeXPos, typeYPos)

static pthread_t displayEvtThread;
static pthread_attr_t displayEvtThreadAttr;
static int displayEvtThreadRunning = 0;
static int numQueriesForDisplay = 0;
DECLARE_QUERY_LIST(app);

static void* displayEvtWork(void *data) {
	Tupel_t *tuple = NULL;
	#ifndef EVALUATION
	struct timeval curTime;
	#endif
	unsigned long long timeUS = 0;

	while (1) {
		sleep(3);
		if (displayEvtThreadRunning == 0) {
			break;
		}

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&curTime,NULL);
	timeUS = (unsigned long long)curTime.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)curTime.tv_usec;
#endif

		tuple = initTupel(timeUS,1);
		if (tuple == NULL) {
			continue;
		}

		srand(time(0));
		//printf("timeStart=%llu, id=%u, tuple=%p\n",timeUS,tuple->id,tuple);
		ACQUIRE_READ_LOCK(slcLock);
		allocItem(SLC_DATA_MODEL,tuple,0,"ui.eventType");
		setItemInt(SLC_DATA_MODEL,tuple,"ui.eventType.xPos",rand() % 1024);
		setItemInt(SLC_DATA_MODEL,tuple,"ui.eventType.yPos",rand() % 1024);
		eventOccuredBroadcast("ui.display",tuple);
		RELEASE_READ_LOCK(slcLock);
	}

	pthread_exit(0);
	return NULL;
}

static void activateDisplay(Query_t *query) {
	numQueriesForDisplay++;

	if (numQueriesForDisplay == 1)  {
		displayEvtThreadRunning = 1;
		pthread_attr_init(&displayEvtThreadAttr);
		pthread_attr_setdetachstate(&displayEvtThreadAttr, PTHREAD_CREATE_JOINABLE);
		if (pthread_create(&displayEvtThread,&displayEvtThreadAttr,displayEvtWork,NULL) < 0) {
			perror("pthread_create displayEvtThread");
		}
	}
}

static void deactivateDisplay(Query_t *query) {
	numQueriesForDisplay--;

	if (numQueriesForDisplay == 0) {
		displayEvtThreadRunning = 0;
		pthread_join(displayEvtThread,NULL);
		pthread_attr_destroy(&displayEvtThreadAttr);
	}
}

static void activateApp(Query_t *query) {
	int listEmpty = 0;
	QuerySelectors_t *querySelec = NULL;

	addAndEnqueueQuery(app,listEmpty, querySelec, query)
	if (listEmpty == 1) {
		/* Shut the fuck up GCC */
		printf("Was empty\n");
	}
}

static void deactivateApp(Query_t *query) {
	int listEmpty = 0;
	QuerySelectors_t *querySelec = NULL, *next = NULL;

	findAndDeleteQuery(app,listEmpty, querySelec, query, next)
	if (listEmpty == 1) {
		/* Shut the fuck up GCC */
		printf("Is now empty\n");
	}
}

static Tupel_t* statusApp(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	Tupel_t *tuple = NULL;
	#ifndef EVALUATION
	struct timeval time;
	#endif
	char *name = NULL;
	unsigned long long timeUS = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	tuple = initTupel(timeUS,1);
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

static Tupel_t* sourceForegroundApp(Selector_t *selectors, int len, Tupel_t* leftTuple) {
	Tupel_t *tuple = NULL;
	#ifndef EVALUATION
	struct timeval time;
	#endif
	char *name = NULL;
	unsigned long long timeUS = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	tuple = initTupel(timeUS,1);
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
	INIT_COMPLEX_TYPE(typeEventType,"eventType",nsUI,2)
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

int onLoad(int argc, char *argv[]) {
	int ret = 0;

	initDatamodel();

	ret = registerProvider(&model, NULL);
	if (ret < 0 ) {
		ERR_MSG("Register provider ui failed: %d\n",-ret);
		return -1;
	}

	INFO_MSG("Registered ui provider\n");
	return 0;
}

int onUnload(void) {
	int ret = 0;

	ret = unregisterProvider(&model, NULL);
	if (ret < 0 ) {
		ERR_MSG("Unregister datamodel ui failed: %d\n",-ret);
	}
	freeDataModel(&model,0);

	INFO_MSG("Unregistered ui provider\n");
	return 0;
}
