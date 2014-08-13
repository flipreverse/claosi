#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

#define REGISTER_QUERIES
//#undef REGISTER_QUERIES

DECLARE_ELEMENTS(nsProcess, model)
DECLARE_ELEMENTS(objProcess, srcUTime, srcSTime)
static void initDatamodel(void);
#ifdef REGISTER_QUERIES
static void setupQueries(void);
static ObjectStream_t processObjCreate, processObjExit, processObjStatus;
static SourceStream_t processUTimeStr;
static Query_t queryFork, queryExit, queryStatus, querySrc;
#endif

static int fork_handler(struct kretprobe_instance *ri, struct pt_regs *regs) {
	int retval = regs_return_value(regs);
	Tupel_t *tupel = NULL;
	struct timeval time;
	unsigned long flags;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	if ((tupel = initTupel(timeUS,1)) == NULL) {
		return 0;
	}

	ACQUIRE_READ_LOCK(slcLock);
	allocItem(SLC_DATA_MODEL,tupel,0,"process.process");
	setItemInt(SLC_DATA_MODEL,tupel,"process.process",retval);
	objectChanged("process.process",tupel,OBJECT_CREATE);
	RELEASE_READ_LOCK(slcLock);

	return 0;
}

static int exit_handler(struct kprobe *p, struct pt_regs *regs) {
	struct task_struct *curTask = current;
	Tupel_t *tupel = NULL;
	struct timeval time;
	unsigned long flags;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	if ((tupel = initTupel(timeUS,1)) == NULL) {
		return 0;
	}

	ACQUIRE_READ_LOCK(slcLock);
	allocItem(SLC_DATA_MODEL,tupel,0,"process.process");
	setItemInt(SLC_DATA_MODEL,tupel,"process.process",curTask->pid);
	objectChanged("process.process",tupel,OBJECT_DELETE);
	RELEASE_READ_LOCK(slcLock);

	return 0;
}

static struct kretprobe forkKP = {
	.kp.symbol_name	= "do_fork",
	.handler		= fork_handler,
	.maxactive		= 20,
};

static struct kprobe exitKP = {
	.symbol_name	= "do_exit",
	.pre_handler = exit_handler
};

static void activate(Query_t *query) {
	int ret = 0;
	
	if ((ret = register_kretprobe(&forkKP)) < 0) {
		ERR_MSG("register_kretprobe (%s) failed. Reason: %d\n",forkKP.kp.symbol_name,ret);
		return;
	}
	DEBUG_MSG(1,"Registered kretprobe at %s\n",forkKP.kp.symbol_name);
	if ((ret = register_kprobe(&exitKP)) < 0) {
		unregister_kretprobe(&forkKP);
		ERR_MSG("register_kprobe (%s) failed. Reason: %d\n",exitKP.symbol_name,ret);
		return;
	}
	DEBUG_MSG(1,"Registered kprobe at %s\n",exitKP.symbol_name);
}

static void deactivate(Query_t *query) {
	unregister_kretprobe(&forkKP);
	DEBUG_MSG(1,"Unregistered kretprobe at %s\n",forkKP.kp.symbol_name);
	unregister_kprobe(&exitKP);
	DEBUG_MSG(1,"Unregistered kprobe at %s\n",exitKP.symbol_name);
}

static Tupel_t* generateStatusObject(Selector_t *selectors, int len) {
	struct task_struct *curTask = NULL;
	Tupel_t *head = NULL, *curTuple = NULL, *prevTuple = NULL;
	struct timeval time;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;

	for_each_process(curTask) {
		if ((curTuple = initTupel(timeUS,1)) == NULL) {
			continue;
		}
		if (head == NULL) {
			head = curTuple;
		}
		// Acquire the slcLock to avoid change in the datamodel while creating the tuple
		allocItem(SLC_DATA_MODEL,curTuple,0,"process.process");
		setItemInt(SLC_DATA_MODEL,curTuple,"process.process",curTask->pid);
		if (prevTuple != NULL) {
			prevTuple->next = curTuple;
		}
		prevTuple = curTuple;
	}

	return head;
}

static Tupel_t* getSrc(Selector_t *selectors, int len) {
	Tupel_t *tuple = NULL;
	struct timeval time;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	tuple = initTupel(timeUS,1);
	if (tuple == NULL) {
		return NULL;
	}

	allocItem(SLC_DATA_MODEL,tuple,0,"process.process.utime");
	setItemInt(SLC_DATA_MODEL,tuple,"process.process.utime",4711);

	return tuple;
};

#ifdef REGISTER_QUERIES
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

static void printResultSource(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	printk("Received tupel with %d items at memory address %p (process duration: %llu us): task utime %d\n",tupel->itemLen,tupel,timeUS - tupel->timestamp, getItemInt(SLC_DATA_MODEL,tupel,"process.process.utime"));
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
	queryStatus.next = &querySrc;
	queryStatus.onQueryCompleted = printResultStatus;
	queryStatus.root = GET_BASE(processObjStatus);
	INIT_OBJ_STREAM(processObjStatus,"process.process",0,0,NULL,OBJECT_STATUS);

	initQuery(&querySrc);
	querySrc.next = NULL;
	querySrc.onQueryCompleted = printResultSource;
	querySrc.root = GET_BASE(processUTimeStr);
	INIT_SRC_STREAM(processUTimeStr,"process.process.utime",0,0,NULL,700);
}
#endif

static void initDatamodel(void) {
	int i = 0;
	INIT_SOURCE_POD(srcUTime,"utime",objProcess,INT,getSrc)
	INIT_SOURCE_POD(srcSTime,"stime",objProcess,INT,getSrc)
	//INIT_SOURCE_COMPLEX(srcProcessSockets,"sockets",objProcess,"net.socket",getSrc) //TODO: Should be an array
	INIT_OBJECT(objProcess,"process",nsProcess,2,INT,activate,deactivate,generateStatusObject)
	ADD_CHILD(objProcess,0,srcUTime)
	ADD_CHILD(objProcess,1,srcSTime)
	//ADD_CHILD(objProcess,2,srcProcessSockets)

	INIT_NS(nsProcess,"process",model,1)
	ADD_CHILD(nsProcess,0,objProcess)

	INIT_MODEL(model,1)
	ADD_CHILD(model,0,nsProcess)
}

int __init process_init(void)
{
	int ret = 0;
	initDatamodel();
#ifdef REGISTER_QUERIES
	setupQueries();
#endif

	if ((ret = registerProvider(&model, NULL)) < 0 ) {
		ERR_MSG("Register provider failed: %d\n",-ret);
		return -1;
	}
#ifdef REGISTER_QUERIES
	if ((ret = registerQuery(&queryFork)) < 0 ) {
		unregisterProvider(&model, NULL);
		ERR_MSG("Register query fork failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Sucessfully registered datamodel for process and queries. Query 'fork' has id: 0x%x, Query 'exit' has id: 0x%x\n",queryFork.queryID,queryExit.queryID);
#endif
	DEBUG_MSG(1,"Registered process provider\n");

	return 0;
}

void __exit process_exit(void) {
	int ret = 0;

#ifdef REGISTER_QUERIES
	if ((ret = unregisterQuery(&queryFork)) < 0 ) {
		ERR_MSG("Unregister query fork failed: %d\n",-ret);
	}
#endif
	if ((ret = unregisterProvider(&model, NULL)) < 0 ) {
		ERR_MSG("Unregister datamodel process failed: %d\n",-ret);
	}

#ifdef REGISTER_QUERIES
	freeOperator(GET_BASE(processObjCreate),0);
	freeOperator(GET_BASE(processObjExit),0);
	freeOperator(GET_BASE(processObjStatus),0);
#endif
	freeDataModel(&model,0);
	DEBUG_MSG(1,"Unregistered process provider\n");
}

module_init(process_init);
module_exit(process_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
