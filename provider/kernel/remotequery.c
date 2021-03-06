#define MSG_FMT(fmt) "[slc-remotequeryk] " fmt
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

static Query_t queryDisplay;
static EventStream_t displayStream;

static void printResult(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS;
	int x = 0, y = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	x = getItemInt(SLC_DATA_MODEL,tupel,"ui.eventType.xPos");
	y = getItemInt(SLC_DATA_MODEL,tupel,"ui.eventType.yPos");
	//printk("timeStart=%llu, timeEnd=%llu, id=%u, tuple=0x%p\n",tupel->timestamp,timeUS,tupel->id,tupel);
	printk("processing duration: %llu us, query id: %u,xPos=%d, yPos=%d\n",timeUS - tupel->timestamp,id,x,y);
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryDisplay);
	queryDisplay.onQueryCompleted = printResult;
	queryDisplay.root = GET_BASE(displayStream);
	INIT_EVT_STREAM(displayStream,"ui.display",0,0,NULL)
}

int __init remotequery_init(void) {
	int ret = 0;

	setupQueries();

	if ((ret = registerQuery(&queryDisplay)) < 0 ) {
		ERR_MSG("Query registration failed: %d\n",-ret);
		return -1;
	}

	return 0;
}

void __exit remotequery_exit(void) {
	int ret = 0;

	if ((ret = unregisterQuery(&queryDisplay)) < 0 ) {
		ERR_MSG("Unregister queries failed: %d\n",-ret);
	}

	freeOperator(GET_BASE(displayStream),0);
}

module_init(remotequery_init);
module_exit(remotequery_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
