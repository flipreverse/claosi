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
#ifndef MSG_FMT
#define MSG_FMT(fmt)	TAG fmt
#endif

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
#define DEBUG_MSG(prio, fmt, ...) do {		\
	if ((prio) <= debug_level) {				\
		PRINT_MSG(MSG_FMT(fmt), ##__VA_ARGS__);			\
	}									\
} while(0);
#endif

#else // !DYNAMIC_DEBUG

#if DEBUG > 0
#define DEBUG_MSG(prio,fmt, ...) do {		\
	if ((prio) <= DEBUG) {				\
		PRINT_MSG(MSG_FMT(fmt), ##__VA_ARGS__);			\
	}									\
} while(0);
#else // ! (DEBUG > 0)
#define DEBUG_MSG(prio,...) do {} while(0);
#endif // DEBUG
#endif // DYNAMIC_DEBUG

#ifdef  __KERNEL__
#define ERR_MSG(fmt, ...) printk(KERN_ERR MSG_FMT(fmt), ##__VA_ARGS__);
#define WARN_MSG(fmt, ...) printk(KERN_WARN MSG_FMT(fmt), ##__VA_ARGS__);
#define INFO_MSG(fmt, ...) printk(KERN_INFO MSG_FMT(fmt), ##__VA_ARGS__);
#else // !__KERNEL__
#define ERR_MSG(fmt, ...) fprintf(stderr,MSG_FMT(fmt), ##__VA_ARGS__);
#define WARN_MSG(fmt, ...) printf(MSG_FMT(fmt), ##__VA_ARGS__);
#define INFO_MSG(fmt, ...) printf(MSG_FMT(fmt), ##__VA_ARGS__);
#endif // __KERNEL__
#else
#define DEBUG_MSG(prio,...) do {} while(0);
#define ERR_MSG(prio,...) do {} while(0);
#define WARN_MSG(prio,...) do {} while(0);
#define INFO_MSG(prio,...) do {} while(0);
#endif

#endif // __OUTPUT_H__
