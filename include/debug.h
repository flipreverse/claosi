#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <common.h>

#define DEBUG 1

#define TAG "[slc] "

#ifdef DEBUG
#if DEBUG > 0
#define DEBUG_MSG(prio,args...) do {                    \
        if ((prio) <= DEBUG) {                          \
                PRINT_MSG(TAG args); \
        }                                               \
} while(0);
#else
#define DEBUG_MSG(prio,...) do {} while(0);
#endif
#else
#define DEBUG_MSG(prio,...) do {} while(0);
#endif

#endif // __DEBUG_H__
