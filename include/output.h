#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include <common.h>

#define OUTPUT 1
//#undef OUTPUT

#define VERBOSE 1
//#undef VERBOSE

#define DEBUG 1

#define DYNAMIC_DEBUG
//#undef DYNAMIC_DEBUG

#ifdef EVALUATION
#undef DYNAMIC_DEBUG
#define DEBUG 0
#endif

#define TAG "[slc] "

#ifdef OUTPUT
#ifdef __KERNEL__
#define PRINT_MSG(args...)					printk(KERN_INFO args);
#else // !__KERNEL__
#define PRINT_MSG(args...)					printf(args);
#endif // __KERNEL__


#ifdef DYNAMIC_DEBUG

#ifdef __KERNEL__
#define DEBUG_MSG(prio,args...) pr_debug(args)
#else  // !__KERNEL__
extern int debug_level;
#define DEBUG_MSG(prio,args...) do {		\
	if ((prio) <= debug_level) {				\
		printf(TAG args);			\
	}									\
} while(0);
#endif

#else // !DYNAMIC_DEBUG

#if DEBUG > 0
#define DEBUG_MSG(prio,args...) do {		\
	if ((prio) <= DEBUG) {				\
		PRINT_DEBUG_MSG_(TAG args);			\
	}									\
} while(0);
#else // ! (DEBUG > 0)
#define DEBUG_MSG(prio,...) do {} while(0);
#endif // DEBUG
#endif // DYNAMIC_DEBUG

#ifdef  __KERNEL__
#define ERR_MSG(args...) printk(KERN_ERR TAG args);
#define WARN_MSG(args...) printk(KERN_WARN TAG args);
#define INFO_MSG(args...) printk(KERN_INFO TAG args);
#else // !__KERNEL__
#define ERR_MSG(args...) fprintf(stderr,TAG args);
#define WARN_MSG(args...) printf(TAG args);
#define INFO_MSG(args...) printf(TAG args);
#endif // __KERNEL__
#else
#define DEBUG_MSG(prio,...) do {} while(0);
#define ERR_MSG(prio,...) do {} while(0);
#define WARN_MSG(prio,...) do {} while(0);
#define INFO_MSG(prio,...) do {} while(0);
#endif

#endif // __OUTPUT_H__
