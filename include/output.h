#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#include <common.h>

#define OUTPUT 1
//#undef OUTPUT

#define VERBOSE 1
//#undef VERBOSE

#define DEBUG 1

#define TAG "[slc] "

#ifdef OUTPUT
	#ifdef DEBUG
		#if DEBUG > 0
			#ifdef __KERNEL__
				#define PRINT_MSG(args...)					printk(KERN_INFO args);
			#else
				#define PRINT_MSG(args...)					printf(args);
			#endif
			#define DEBUG_MSG(prio,args...) do {		\
					if ((prio) <= DEBUG) {				\
						PRINT_MSG(TAG args);			\
					}									\
			} while(0);
		#else
			#define DEBUG_MSG(prio,...) do {} while(0);
		#endif
	#else
		#define DEBUG_MSG(prio,...) do {} while(0);
	#endif
	#ifdef  __KERNEL__
		#define ERR_MSG(args...) printk(KERN_ERR TAG args);
		#define WARN_MSG(args...) printk(KERN_WARN TAG args);
	#else
		#define ERR_MSG(args...) fprintf(stderr,TAG args);
		#define WARN_MSG(args...) printf(TAG args);
	#endif
#else
	#define DEBUG_MSG(prio,...) do {} while(0);
	#define ERR_MSG(prio,...) do {} while(0);
	#define WARN_MSG(prio,...) do {} while(0);
#endif

#endif // __OUTPUT_H__
