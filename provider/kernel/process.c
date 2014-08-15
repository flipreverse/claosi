#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

DECLARE_ELEMENTS(nsProcess, model)
DECLARE_ELEMENTS(objProcess, srcUTime, srcSTime, srcComm)

DECLARE_QUERY_LIST(fork)
DECLARE_QUERY_LIST(exit)

static struct kretprobe forkKP;
static char forkSymbolName[] = "do_fork";
static struct kprobe exitKP;
static char exitSymbolName[] = "do_exit";

static int handlerFork(struct kretprobe_instance *ri, struct pt_regs *regs) {
	int retval = regs_return_value(regs);
	Tupel_t *tuple = NULL;
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	struct timeval time;
	unsigned long flags;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;

	forEachQueryObject(slcLock, fork, pos, querySelec, OBJECT_CREATE)
		tuple = initTupel(timeUS,1);
		if (tuple == NULL) {
			continue;
		}
		allocItem(SLC_DATA_MODEL,tuple,0,"process.process");
		setItemInt(SLC_DATA_MODEL,tuple,"process.process",retval);
		objectChangedUnicast(querySelec->query,tuple);
	endForEachQueryObject(slcLock,fork)

	return 0;
}

static int handlerExit(struct kprobe *p, struct pt_regs *regs) {
	struct task_struct *curTask = current;
	Tupel_t *tuple = NULL;
	struct list_head *pos = NULL;
	QuerySelectors_t *querySelec = NULL;
	struct timeval time;
	unsigned long flags;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;

	forEachQueryObject(slcLock, exit, pos, querySelec, OBJECT_DELETE)
		tuple = initTupel(timeUS,1);
		if (tuple == NULL) {
			continue;
		}
		allocItem(SLC_DATA_MODEL,tuple,0,"process.process");
		setItemInt(SLC_DATA_MODEL,tuple,"process.process",curTask->pid);
		objectChangedUnicast(querySelec->query,tuple);
	endForEachQueryObject(slcLock,exit)

	return 0;
}

static void activateProcess(Query_t *query) {
	int ret = 0, events = 0;
	QuerySelectors_t *querySelec = NULL;

	events = ((ObjectStream_t*)query->root)->objectEvents;
	if ((events & OBJECT_CREATE) == OBJECT_CREATE) {
		addAndEnqueueQuery(fork,ret, querySelec, query)
		if (ret == 1) {
			memset(&forkKP,0,sizeof(struct kretprobe));
			forkKP.kp.symbol_name = forkSymbolName;
			forkKP.handler = handlerFork;
			forkKP.maxactive = 20;
			ret = register_kretprobe(&forkKP);
			if (ret < 0) {
				ERR_MSG("Registration of kprobe at %s failed. Reason: %d\n",forkKP.kp.symbol_name,ret);
			} else {
				DEBUG_MSG(1,"Registered kretprobe at %s\n",forkKP.kp.symbol_name);
			}
		}
	}
	if ((events & OBJECT_DELETE) == OBJECT_DELETE) {
		addAndEnqueueQuery(exit,ret, querySelec, query)
		if (ret == 1) {
			memset(&exitKP,0,sizeof(struct kprobe));
			exitKP.symbol_name = exitSymbolName;
			exitKP.pre_handler = handlerExit;
			ret = register_kprobe(&exitKP);
			if (ret < 0) {
				ERR_MSG("Registration of kprobe at %s failed. Reason: %d\n",exitKP.symbol_name,ret);
				return;
			} else {
				DEBUG_MSG(1,"Registered kprobe at %s\n",exitKP.symbol_name);
			}
		}
	}
}

static void deactivateProcess(Query_t *query) {
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;
	int events = 0, listEmpty = 0;

	events = ((ObjectStream_t*)query->root)->objectEvents;
	if ((events & OBJECT_CREATE) == OBJECT_CREATE) {
		findAndDeleteQuery(fork,listEmpty, querySelec, query, pos, next);
		if (listEmpty == 1) {
			unregister_kretprobe(&forkKP);
			DEBUG_MSG(1,"Unregistered kretprobe at %s. Missed it %lu times.\n",forkKP.kp.symbol_name,forkKP.kp.nmissed);
		}
	}
	if ((events & OBJECT_DELETE) == OBJECT_DELETE) {
		findAndDeleteQuery(exit,listEmpty, querySelec, query, pos, next)
		if (listEmpty == 1) {
			unregister_kprobe(&exitKP);
			DEBUG_MSG(1,"Unregistered kprobe at %s. Missed it %lu times.\n",exitKP.symbol_name,exitKP.nmissed);
		}
	}
}

static Tupel_t* generateProcessStatus(Selector_t *selectors, int len) {
	struct task_struct *curTask = NULL;
	Tupel_t *head = NULL, *curTuple = NULL, *prevTuple = NULL;
	struct timeval time;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;

	for_each_process(curTask) {
		curTuple = initTupel(timeUS,1);
		if (curTuple == NULL) {
			continue;
		}
		if (head == NULL) {
			head = curTuple;
		}
		allocItem(SLC_DATA_MODEL,curTuple,0,"process.process");
		setItemInt(SLC_DATA_MODEL,curTuple,"process.process",curTask->pid);
		if (prevTuple != NULL) {
			prevTuple->next = curTuple;
		}
		prevTuple = curTuple;
	}

	return head;
}

static Tupel_t* getComm(Selector_t *selectors, int len) {
	Tupel_t *tuple = NULL;
	struct timeval time;
	unsigned long long timeUS = 0;
	struct pid *pid = NULL;
	struct task_struct *task = NULL;
	char *comm = NULL;

	if (selectors == NULL) {
		return NULL;
	}
	// Retrieve the kernel representation of a pid
	pid = find_get_pid(*(int*)(&selectors[0].value));
	if (pid == NULL) {
		return NULL;
	}
	// Resolve it to a struct task_struct
	task = get_pid_task(pid,PIDTYPE_PID);
	if (task == NULL) {
		return NULL;
	}
	// .. and copy the comm
	comm = ALLOC(strlen(task->comm) + 1);
	if (comm == NULL) {
		return NULL;
	}
	strcpy(comm,task->comm);
	// Give it back to the kernel
	put_task_struct(task);

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	tuple = initTupel(timeUS,1);
	if (tuple == NULL) {
		return NULL;
	}

	allocItem(SLC_DATA_MODEL,tuple,0,"process.process.comm");
	setItemString(SLC_DATA_MODEL,tuple,"process.process.comm",comm);

	return tuple;
}

static Tupel_t* getSTime(Selector_t *selectors, int len) {
	Tupel_t *tuple = NULL;
	struct timeval time;
	unsigned long long timeUS = 0;
	struct pid *pid = NULL;
	struct task_struct *task = NULL;
	unsigned int sTimeUS = 0;

	if (selectors == NULL) {
		return NULL;
	}
	// Retrieve the kernel representation of a pid
	pid = find_get_pid(*(int*)(&selectors[0].value));
	if (pid == NULL) {
		return NULL;
	}
	// Resolve it to a struct task_struct
	task = get_pid_task(pid,PIDTYPE_PID);
	if (task == NULL) {
		return NULL;
	}
	// .. and read the stime
	sTimeUS = jiffies_to_usecs(task->stime);
	// Give it back to the kernel
	put_task_struct(task);

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	tuple = initTupel(timeUS,1);
	if (tuple == NULL) {
		return NULL;
	}

	allocItem(SLC_DATA_MODEL,tuple,0,"process.process.stime");
	setItemInt(SLC_DATA_MODEL,tuple,"process.process.stime",sTimeUS);

	return tuple;
}

static Tupel_t* getUTime(Selector_t *selectors, int len) {
	Tupel_t *tuple = NULL;
	struct timeval time;
	unsigned long long timeUS = 0;
	struct pid *pid = NULL;
	struct task_struct *task = NULL;
	unsigned int uTimeUS = 0;

	if (selectors == NULL) {
		return NULL;
	}
	// Retrieve the kernel representation of a pid
	pid = find_get_pid(*(int*)(&selectors[0].value));
	if (pid == NULL) {
		return NULL;
	}
	// Resolve it to a struct task_struct
	task = get_pid_task(pid,PIDTYPE_PID);
	if (task == NULL) {
		return NULL;
	}
	// .. and read the utime
	uTimeUS = jiffies_to_usecs(task->utime);
	// Give it back to the kernel
	put_task_struct(task);
	//printk("task=%d, comm=%s, utime=%u(%lu), stime=%u(%lu)\n",task->pid,task->comm,jiffies_to_usecs(task->utime),task->utime,jiffies_to_usecs(task->stime),task->stime);

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	tuple = initTupel(timeUS,1);
	if (tuple == NULL) {
		return NULL;
	}

	allocItem(SLC_DATA_MODEL,tuple,0,"process.process.utime");
	setItemInt(SLC_DATA_MODEL,tuple,"process.process.utime",uTimeUS);

	return tuple;
}

static void initDatamodel(void) {
	int i = 0;
	INIT_SOURCE_POD(srcUTime,"utime",objProcess,INT,getUTime)
	INIT_SOURCE_POD(srcSTime,"stime",objProcess,INT,getSTime)
	INIT_SOURCE_POD(srcComm,"comm",objProcess,STRING,getComm)
	//INIT_SOURCE_COMPLEX(srcProcessSockets,"sockets",objProcess,"net.socket",getSrc) //TODO: Should be an array
	INIT_OBJECT(objProcess,"process",nsProcess,3,INT,activateProcess,deactivateProcess,generateProcessStatus)
	ADD_CHILD(objProcess,0,srcUTime)
	ADD_CHILD(objProcess,1,srcSTime)
	ADD_CHILD(objProcess,2,srcComm)
	//ADD_CHILD(objProcess,3,srcProcessSockets)

	INIT_NS(nsProcess,"process",model,1)
	ADD_CHILD(nsProcess,0,objProcess)

	INIT_MODEL(model,1)
	ADD_CHILD(model,0,nsProcess)
}

int __init process_init(void)
{
	int ret = 0;
	initDatamodel();

	if ((ret = registerProvider(&model, NULL)) < 0 ) {
		ERR_MSG("Register provider failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Registered process provider\n");

	return 0;
}

void __exit process_exit(void) {
	int ret = 0;

	ret = unregisterProvider(&model, NULL);
	if (ret < 0 ) {
		ERR_MSG("Unregister datamodel process failed: %d\n",-ret);
	}
	freeDataModel(&model,0);

	DEBUG_MSG(1,"Unregistered process provider\n");
}

module_init(process_init);
module_exit(process_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
