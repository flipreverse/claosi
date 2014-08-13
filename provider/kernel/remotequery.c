#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/if_ether.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>

static Query_t queryDisplay;
static EventStream_t displayStream;
static Predicate_t xPosPred, yPosPred;
static Filter_t posFilter;

static void printResult(unsigned int id, Tupel_t *tupel) {
	struct timeval time;
	unsigned long long timeUS;
	int x = 0, y = 0;

	do_gettimeofday(&time);
	x = getItemInt(SLC_DATA_MODEL,tupel,"ui.eventType.xPos");
	y = getItemInt(SLC_DATA_MODEL,tupel,"ui.eventType.yPos");
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
	//printk("timeStart=%llu, timeEnd=%llu, id=%u, tuple=0x%p\n",tupel->timestamp,timeUS,tupel->id,tupel);
	printk("processing duration: %llu us, query id: %u,xPos=%d, yPos=%d\n",timeUS - tupel->timestamp,id,x,y);
	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryDisplay);
	queryDisplay.onQueryCompleted = printResult;
	queryDisplay.root = GET_BASE(displayStream);
	INIT_EVT_STREAM(displayStream,"ui.display",0,0,NULL) /*GET_BASE(posFilter))
	INIT_FILTER(posFilter,NULL,2)
	ADD_PREDICATE(posFilter,0,xPosPred)
	SET_PREDICATE(xPosPred,GEQ, STREAM, "ui.eventType.xPos", POD, "700")
	ADD_PREDICATE(posFilter,1,yPosPred)
	SET_PREDICATE(yPosPred,LEQ, STREAM, "ui.eventType.yPos", POD, "300")*/
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
