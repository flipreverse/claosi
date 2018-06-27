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
#include "eval-txrx.c"
#include "eval-txrxbytes.c"

static Query_t *firstQuery = NULL;

int __init evalqueries_3_init(void) {
	int ret = 0;

	setupQueriesTXRX(&firstQuery);
	setupQueriesTXRXBytes(&firstQuery);

	if (useRelayFS) {
		if (initRelayFS() < 0) {
			return -1;
		}
	}

	ret = registerQuery(firstQuery);
	if (ret < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		return -1;
	}
	INFO_MSG("Registered eval net queries\n");

	return 0;
}

void __exit evalqueries_3_exit(void) {
	int ret = 0;

	ret = unregisterQuery(firstQuery);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval net failed: %d\n",-ret);
	}

	if (useRelayFS) {
		destroyRelayFS();
	}

	destroyQueriesTXRX();
	destroyQueriesTXRXBytes();

	INFO_MSG("Unregistered eval net queries\n");
}

module_init(evalqueries_3_init);
module_exit(evalqueries_3_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
