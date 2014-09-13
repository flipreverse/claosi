#include <linux/module.h>
#include <linux/relay.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <datamodel.h>
#include <query.h>
#include <api.h>
#include <evaluation.h>

#include "eval-relay.c"
#include "eval-txrx.c"
#include "eval-txrxjoin.c"

static Query_t *firstQuery = NULL;

int __init evalqueries_2_init(void) {
	int ret = 0;

	setupQueriesTXRX(&firstQuery);
	setupQueriesTXRXJoin(&firstQuery);

	if (initRelayFS() < 0) {
		return -1;
	}
	ret = registerQuery(firstQuery);
	if (ret < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Registered eval net queries\n");

	return 0;
}

void __exit evalqueries_2_exit(void) {
	int ret = 0;

	ret = unregisterQuery(firstQuery);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval net failed: %d\n",-ret);
	}

	destroyRelayFS();

	destroyQueriesTXRX();
	destroyQueriesTXRXJoin();
	DEBUG_MSG(1,"Unregistered eval net queries\n");
}

module_init(evalqueries_2_init);
module_exit(evalqueries_2_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
