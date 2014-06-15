#include <linux/module.h>

#include <datamodel.h>
#include <query.h>
#include <api.h>

static int __init slc_init(void) {
	if (initSLC() == -1) {
		return -1;
	}
	DEBUG_MSG(1,"Initialized SLC\n");
	return 0;
}

static void __exit slc_exit(void) {
	destroySLC();
	DEBUG_MSG(1,"Destroyed SLC\n");
}

module_init(slc_init);
module_exit(slc_exit);

MODULE_AUTHOR("Alexander Lochmann (alexander.lochmann@tu-dortmund.de)");
MODULE_DESCRIPTION("");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

