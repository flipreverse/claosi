#include <linux/module.h>
#include <linux/relay.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>
#include <evaluation.h>
#include <linux/proc_fs.h>

static ObjectStream_t processObjExit, processObjExitJoin;
static Join_t joinCommFork, joinStimeFork;
static Predicate_t commPredicateFork, commPredicateFork;
static Query_t queryExitJoin, queryExit;

static int useRelayFS = 0;
module_param(useRelayFS, int, S_IRUGO);


static struct rchan *relayfsOutputExit, *relayfsOutputExitJoin;
static struct dentry *relayfsDirExit = NULL, *relayfsDirExitJoin = NULL;

static struct dentry *create_buf_file_handler(const char *filename,
	struct dentry *parent,
	umode_t mode,
	struct rchan_buf *buf,
	int *is_global) {

	return debugfs_create_file(filename, 0777, parent, buf,&relay_file_operations);
}

static int remove_buf_file_handler(struct dentry *dentry) {

	debugfs_remove(dentry);
	return 0;
}

static struct rchan_callbacks relay_callbacks = {
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

static int initRelayFS(void) {
	relayfsDirExit = debugfs_create_dir("exit",NULL);
	if (relayfsDirExit == NULL) {
		ERR_MSG("Cannot create debugfs direcotry %s\n",RELAYFS_DIR);
		return -1;
	}
	relayfsOutputExit = relay_open(RELAYFS_NAME, relayfsDirExit, SUBBUF_SIZE, N_SUBBUFS,&relay_callbacks,NULL);
	if (relayfsOutputExit == NULL) {
		ERR_MSG("Cannot create relayfs %s\n",RELAYFS_NAME);
		debugfs_remove(relayfsDirExit);
		return -1;
	}
	relayfsDirExitJoin = debugfs_create_dir("exitjoin",NULL);
	if (relayfsDirExitJoin == NULL) {
		ERR_MSG("Cannot create debugfs direcotry %s\n",RELAYFS_DIR);
		relay_close(relayfsOutputExit);
		debugfs_remove(relayfsDirExit);
		return -1;
	}
	relayfsOutputExitJoin = relay_open(RELAYFS_NAME, relayfsDirExitJoin, SUBBUF_SIZE, N_SUBBUFS,&relay_callbacks,NULL);
	if (relayfsOutputExitJoin == NULL) {
		ERR_MSG("Cannot create relayfs %s\n",RELAYFS_NAME);
		relay_close(relayfsOutputExit);
		debugfs_remove(relayfsDirExit);
		debugfs_remove(relayfsDirExitJoin);
		return -1;
	}
	return 0;
}

static void destroyRelayFS(void) {
	relay_close(relayfsOutputExitJoin);
	relay_close(relayfsOutputExit);
	debugfs_remove(relayfsDirExit);
	debugfs_remove(relayfsDirExitJoin);
}

static void printResultExitJoin(unsigned int id, Tupel_t *tupel) {
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
		relay_write(relayfsOutputExitJoin,&sample,sizeof(sample));
	}

	freeTupel(SLC_DATA_MODEL,tupel);
}


static void printResultExit(unsigned int id, Tupel_t *tupel) {
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
		relay_write(relayfsOutputExit,&sample,sizeof(sample));
	}

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void initQueriesFork(void) {
	initQuery(&queryExitJoin);
	queryExitJoin.onQueryCompleted = printResultExitJoin;
	queryExitJoin.root = GET_BASE(processObjExitJoin);
	queryExitJoin.next = & queryExit;
	INIT_OBJ_STREAM(processObjExitJoin,"process.process",0,0,GET_BASE(joinStimeFork),OBJECT_DELETE);
	INIT_JOIN(joinStimeFork,"process.process.stime", GET_BASE(joinCommFork),1)
	ADD_PREDICATE(joinStimeFork,0,commPredicateFork)
	SET_PREDICATE(commPredicateFork,EQUAL, OP_STREAM, "process.process", OP_JOIN, "process.process")
	INIT_JOIN(joinCommFork,"process.process.comm", NULL,1)
	ADD_PREDICATE(joinCommFork,0,commPredicateFork)
	SET_PREDICATE(commPredicateFork,EQUAL, OP_STREAM, "process.process", OP_JOIN, "process.process")

	initQuery(&queryExit);
	queryExit.onQueryCompleted = printResultExit;
	queryExit.root = GET_BASE(processObjExit);
	//queryExitJoin.next = & querySockets;
	INIT_OBJ_STREAM(processObjExit,"process.process",0,0,NULL,OBJECT_DELETE);
}

static char localBuffer[20];

static ssize_t enableQueryWrite(struct file *file, const char *buffer, size_t count, loff_t *offset) {

	int toCopy = count, value, ret;

	if (toCopy > 20) {
		toCopy = 20;
	}
	if (copy_from_user(localBuffer, buffer, toCopy)) {
		return -EFAULT;
	}
	localBuffer[19] = '\0';
	if (kstrtos32(localBuffer,10,&value) < 0) {
		return -EINVAL;
	}
	if (value == 1) {
		ret = registerQuery(&queryExitJoin);
		if (ret < 0 ) {
			ERR_MSG("Register failed: %d\n",-ret);
			destroyRelayFS();
			return -1;
		}
		DEBUG_MSG(1,"Registered eval exit queries\n");
	} else if (value == 0) {
		ret = unregisterQuery(&queryExitJoin);
		if (ret < 0 ) {
			ERR_MSG("Unregister eval exit failed: %d\n",-ret);
		}
		DEBUG_MSG(1,"Unregistered eval exit queries\n");
	}

	return toCopy;
}

static const struct file_operations enableQueryOps = {
	.write = enableQueryWrite,
};

static struct proc_dir_entry *procfsEnableQuery;

int __init evalqueries_4_init(void) {
	
	initQueriesFork();

	if (useRelayFS) {
		if (initRelayFS() < 0) {
			return -1;
		}
	}
	procfsEnableQuery = proc_create("enableQuery",0777,NULL,&enableQueryOps);
	if (procfsEnableQuery == NULL) {
		printk("Cannot create procfs 'enableQuery'\n");
		if (useRelayFS) {
			destroyRelayFS();
		}
	}

	return 0;
}

void __exit evalqueries_4_exit(void) {

	if (useRelayFS) {
		destroyRelayFS();
	}
	proc_remove(procfsEnableQuery);

	freeOperator(GET_BASE(processObjExit),0);
	freeOperator(GET_BASE(processObjExitJoin),0);
}

module_init(evalqueries_4_init);
module_exit(evalqueries_4_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
