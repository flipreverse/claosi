#define MSG_FMT(fmt) "[slc-eval2] " fmt
#include <linux/module.h>
#include <linux/relay.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>
#include <evaluation.h>

static char *devName = "eth1";
module_param(devName, charp, S_IRUGO);
static int useRelayFS = 0;
module_param(useRelayFS, int, S_IRUGO);

#include "eval-relay.c"
#include "eval-txrxjoin.c"

static Query_t *firstQuery = NULL;

int __init evalqueries_2_init(void) {
	int ret = 0;

	setupQueriesTXRXJoin(&firstQuery);

	if (useRelayFS) {
		if (initRelayFS() < 0) {
			return -1;
		}
	}

	ret = registerQuery(firstQuery);
	if (ret < 0 ) {
		ERR_MSG("Register eval tx/rx + join failed: %d\n",-ret);
		return -1;
	}
	INFO_MSG("Registered eval tx/rx + join queries\n");

	return 0;
}

void __exit evalqueries_2_exit(void) {
	int ret = 0;

	ret = unregisterQuery(firstQuery);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval tx/rx + join failed: %d\n",-ret);
	}

	if (useRelayFS) {
		destroyRelayFS();
	}

	destroyQueriesTXRXJoin();

	INFO_MSG("Unregistered eval tx/rx + join queries\n");
}

module_init(evalqueries_2_init);
module_exit(evalqueries_2_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
