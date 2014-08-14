#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

DECLARE_ELEMENTS(nsProcess, model)
DECLARE_ELEMENTS(objProcess, srcUTime, srcSTime)
static int forkProbeActive = 0;
static LIST_HEAD(forkQueriesList);
static DEFINE_SPINLOCK(forkListLock);
static int exitProbeActive = 0;
static LIST_HEAD(exitQueriesList);
static DEFINE_SPINLOCK(exitListLock);

static int handlerFork(struct kretprobe_instance *ri, struct pt_regs *regs) {
	int retval = regs_return_value(regs);
	Tupel_t *tuple = NULL;
	struct timeval time;
	unsigned long flags;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	tuple = initTupel(timeUS,1);
	if (tuple == NULL) {
		return 0;
	}

	ACQUIRE_READ_LOCK(slcLock);
	allocItem(SLC_DATA_MODEL,tuple,0,"process.process");
	setItemInt(SLC_DATA_MODEL,tuple,"process.process",retval);
	objectChanged("process.process",tuple,OBJECT_CREATE);
	RELEASE_READ_LOCK(slcLock);

	return 0;
}

static int handlerExit(struct kprobe *p, struct pt_regs *regs) {
	struct task_struct *curTask = current;
	Tupel_t *tupel = NULL;
	struct timeval time;
	unsigned long flags;
	unsigned long long timeUS = 0;

	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	tuple = initTupel(timeUS,1);
	if (tuple == NULL) {
		return 0;
	}

	ACQUIRE_READ_LOCK(slcLock);
	allocItem(SLC_DATA_MODEL,tuple,0,"process.process");
	setItemInt(SLC_DATA_MODEL,tuple,"process.process",curTask->pid);
	objectChanged("process.process",tuple,OBJECT_DELETE);
	RELEASE_READ_LOCK(slcLock);

	return 0;
}

static struct kretprobe forkKP = {
	.kp.symbol_name	= "do_fork",
	.handler		= handlerFork,
	.maxactive		= 20,
};

static struct kprobe exitKP = {
	.symbol_name	= "do_exit",
	.pre_handler = handlerExit
};

static void activateProcess(Query_t *query) {
	int ret = 0, events = 0;
	QuerySelectors_t *querySelec = NULL;

	events = ((ObjectStream_t*)query->root)->objectEvents;
	if ((events & OBJECT_CREATE) == OBJECT_CREATE) {
		querySelec = (QuerySelectors_t*)ALLOC(sizeof(QuerySelectors_t));
		if (querySelec == NULL) {
			return;
		}
		querySelec->query = query;
		spin_lock(&forkListLock);
		list_add_tail(&querySelec->list,&forkQueriesList);
		spin_unlock(&forkListLock);
		if (forkProbeActive == 0) {
			ret = register_kprobe(&forkKP);
			if (ret < 0) {
				ERR_MSG("register_kprobe failed. Reason: %d\n",ret);
			} else {
				forkProbeActive = 1;
				DEBUG_MSG(1,"Registered kprobe at %s\n",forkKP.symbol_name);
			}
		}
	}
	if ((events & OBJECT_DELETE) == OBJECT_DELETE) {
		querySelec = (QuerySelectors_t*)ALLOC(sizeof(QuerySelectors_t));
		if (querySelec == NULL) {
			return;
		}
		querySelec->query = query;
		spin_lock(&exitListLock);
		list_add_tail(&querySelec->list,&exitQueriesList);
		spin_unlock(&exitListLock);
		if (exitProbeActive == 0) {
			ret = register_kprobe(&exitKP);
			if (ret < 0) {
				ERR_MSG("register_kprobe failed. Reason: %d\n",ret);
				return;
			} else {
				exitProbeActive = 1;
				DEBUG_MSG(1,"Registered kprobe at %s\n",exitKP.symbol_name);
			}
		}
	}
}

static void deactivateProcess(Query_t *query) {
	struct list_head *pos = NULL, *next = NULL;
	QuerySelectors_t *querySelec = NULL;
	int events = 0;

	events = ((ObjectStream_t*)query->root)->objectEvents;
	if ((events & OBJECT_CREATE) == OBJECT_CREATE) {
		spin_lock(&forkListLock);
		list_for_each_safe(pos,next,&forkQueriesList) {
			querySelec = container_of(pos,QuerySelectors_t,list);
			if (querySelec->query == query) {
				list_del(&querySelec->list);
				break;
			}
		}
		spin_unlock(&forkListLock);
		if (list_empty(&forkQueriesList)) {
			if (forkProbeActive == 1) {
				unregister_kprobe(&forkKP);
				DEBUG_MSG(1,"Unregistered kprobes at %s\n",forkKP.symbol_name);
				forkProbeActive = 0;
			}
		}
	}
	if ((events & OBJECT_DELETE) == OBJECT_DELETE) {
		spin_lock(&exitListLock);
		list_for_each_safe(pos,next,&exitQueriesList) {
			querySelec = container_of(pos,QuerySelectors_t,list);
			if (querySelec->query == query) {
				list_del(&querySelec->list);
				break;
			}
		}
		spin_unlock(&exitListLock);
		if (list_empty(&exitQueriesList)) {
			if (exitProbeActive == 1) {
				unregister_kprobe(&exitKP);
				DEBUG_MSG(1,"Unregistered kprobes at %s\n",exitKP.symbol_name);
				exitProbeActive = 0;
			}
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
}

static void initDatamodel(void) {
	int i = 0;
	INIT_SOURCE_POD(srcUTime,"utime",objProcess,INT,getUTime)
	INIT_SOURCE_POD(srcSTime,"stime",objProcess,INT,getSTime)
	//INIT_SOURCE_COMPLEX(srcProcessSockets,"sockets",objProcess,"net.socket",getSrc) //TODO: Should be an array
	INIT_OBJECT(objProcess,"process",nsProcess,2,INT,activateProcess,deactivateProcess,generateProcessStatus)
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
