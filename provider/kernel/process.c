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
static void initQuery(void);
static ObjectStream_t processObjCreate, processObjExit, processObjStatus;
static Query_t queryFork, queryExit, queryStatus;
#endif

static int fork_handler(struct kretprobe_instance *ri, struct pt_regs *regs) {
	int retval = regs_return_value(regs);
	Tupel_t *tupel = NULL;
	struct timeval time;
	unsigned long flags;

	do_gettimeofday(&time);
	if ((tupel = initTupel(time.tv_sec * USEC_PER_MSEC + time.tv_usec,1)) == NULL) {
		return 0;
	}

	ACQUIRE_READ_LOCK(slcLock);
	allocItem(slcDataModel,tupel,0,"process.process");
	setItemInt(slcDataModel,tupel,"process.process",retval);
	RELEASE_READ_LOCK(slcLock);
	objectChanged("process.process",tupel,OBJECT_CREATE);

	return 0;
}

static int exit_handler(struct kprobe *p, struct pt_regs *regs) {
	struct task_struct *curTask = current;
	Tupel_t *tupel = NULL;
	struct timeval time;
	unsigned long flags;

	do_gettimeofday(&time);
	if ((tupel = initTupel(time.tv_sec * USEC_PER_MSEC + time.tv_usec,1)) == NULL) {
		return 0;
	}

	ACQUIRE_READ_LOCK(slcLock);
	allocItem(slcDataModel,tupel,0,"process.process");
	setItemInt(slcDataModel,tupel,"process.process",curTask->pid);
	RELEASE_READ_LOCK(slcLock);
	objectChanged("process.process",tupel,OBJECT_DELETE);

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

static void activate(void) {
	int ret = 0;
	
	if ((ret = register_kretprobe(&forkKP)) < 0) {
		DEBUG_MSG(1,",register_kretprobe (%s) failed. Reason: %d\n",forkKP.kp.symbol_name,ret);
		return;
	}
	DEBUG_MSG(1,"Registered kretprobe at %s\n",forkKP.kp.symbol_name);
	if ((ret = register_kprobe(&exitKP)) < 0) {
		unregister_kretprobe(&forkKP);
		DEBUG_MSG(1,",register_kprobe (%s) failed. Reason: %d\n",exitKP.symbol_name,ret);
		return;
	}
	DEBUG_MSG(1,"Registered kprobe at %s\n",exitKP.symbol_name);
}

static void deactivate(void) {
	unregister_kretprobe(&forkKP);
	DEBUG_MSG(1,"Unregistered kretprobe at %s\n",forkKP.kp.symbol_name);
	unregister_kprobe(&exitKP);
	DEBUG_MSG(1,"Unregistered kprobe at %s\n",exitKP.symbol_name);
}

static Tupel_t* generateStatusObject(void) {
	struct task_struct *curTask = NULL;
	Tupel_t *head = NULL, *curTuple = NULL, *prevTuple = NULL;
	struct timeval time;
	unsigned long long timeMS = 0;
	unsigned long flags;

	do_gettimeofday(&time);
	timeMS = time.tv_sec * USEC_PER_MSEC + time.tv_usec;

	for_each_process(curTask) {
		if ((curTuple = initTupel(timeMS,1)) == NULL) {
			continue;
		}
		if (head == NULL) {
			head = curTuple;
		}
		// Acquire the slcLock to avoid change in the datamodel while creating the tuple
		ACQUIRE_READ_LOCK(slcLock);
		allocItem(slcDataModel,curTuple,0,"process.process");
		setItemInt(slcDataModel,curTuple,"process.process",curTask->pid);
		RELEASE_READ_LOCK(slcLock);
		if (prevTuple != NULL) {
			prevTuple->next = curTuple;
		}
		prevTuple = curTuple;
	}

	return head;
}

static void* getSrc(void) {
	return NULL;
};

#ifdef REGISTER_QUERIES
static void printResultFork(QueryID_t id, Tupel_t *tupel) {
	struct timeval time;

	do_gettimeofday(&time);
	printk("Received tupel with %d items at memory address %p at %lu us: task %d created\n",tupel->itemLen,tupel,time.tv_sec * USEC_PER_MSEC + time.tv_usec, getItemInt(slcDataModel,tupel,"process.process"));
	freeTupel(slcDataModel,tupel);
}

static void printResultExit(QueryID_t id, Tupel_t *tupel) {
	struct timeval time;

	do_gettimeofday(&time);
	printk("Received tupel with %d items at memory address %p at %lu us: task %d terminated\n",tupel->itemLen,tupel,time.tv_sec * USEC_PER_MSEC + time.tv_usec, getItemInt(slcDataModel,tupel,"process.process"));
	freeTupel(slcDataModel,tupel);
}

static void printResultStatus(QueryID_t id, Tupel_t *tupel) {
	struct timeval time;

	do_gettimeofday(&time);
	printk("Received tupel with %d items at memory address %p, created at %llu, received at %lu us: task %d status\n",tupel->itemLen,tupel,tupel->timestamp,time.tv_sec * USEC_PER_MSEC + time.tv_usec, getItemInt(slcDataModel,tupel,"process.process"));
	freeTupel(slcDataModel,tupel);
}

static void initQuery(void) {
	queryFork.next = &queryExit;
	queryFork.queryType = ASYNC;
	queryFork.queryID = 0;
	queryFork.onQueryCompleted = printResultFork;
	queryFork.root = GET_BASE(processObjCreate);
	INIT_OBJ_STREAM(processObjCreate,"process.process",0,NULL,OBJECT_CREATE);

	queryExit.next = &queryStatus;
	queryExit.queryType = ASYNC;
	queryExit.queryID = 0;
	queryExit.onQueryCompleted = printResultExit;
	queryExit.root = GET_BASE(processObjExit);
	INIT_OBJ_STREAM(processObjExit,"process.process",0,NULL,OBJECT_DELETE);

	queryStatus.next = NULL;
	queryStatus.queryType = ASYNC;
	queryStatus.queryID = 0;
	queryStatus.onQueryCompleted = printResultStatus;
	queryStatus.root = GET_BASE(processObjStatus);
	INIT_OBJ_STREAM(processObjStatus,"process.process",0,NULL,OBJECT_STATUS);
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
	initQuery();
#endif

	if ((ret = registerProvider(&model, NULL)) < 0 ) {
		DEBUG_MSG(1,"Register provider failed: %d\n",-ret);
		return -1;
	}
#ifdef REGISTER_QUERIES
	if ((ret = registerQuery(&queryFork)) < 0 ) {
		unregisterProvider(&model, NULL);
		DEBUG_MSG(1,"Register query fork failed: %d\n",-ret);
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
		DEBUG_MSG(1,"Unregister query fork failed: %d\n",-ret);
	}
#endif
	if ((ret = unregisterProvider(&model, NULL)) < 0 ) {
		DEBUG_MSG(1,"Unregister datamodel process failed: %d\n",-ret);
	}

#ifdef REGISTER_QUERIES
	freeQuery(GET_BASE(processObjCreate),0);
	freeQuery(GET_BASE(processObjExit),0);
	freeQuery(GET_BASE(processObjStatus),0);
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
