#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __KERNEL__
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/rwlock.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/hrtimer.h>
#include <asm/page.h>
#else
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#define PAGE_SIZE 4096
#endif

#define PAGE_ORDER							5
#define NUM_PAGES							(1 << PAGE_ORDER)
#define MAX_NAME_LEN						40
#define DECLARE_BUFFER(name)				char name[MAX_NAME_LEN + 1];
#define PROCFS_DIR_NAME						"slc"
#define PROCFS_LOCKFILE						"lock"
#define PROCFS_DATAMODELFILE				"datamodel"
#define SLC_DATA_MODEL						(slcDataModel)
#define REWRITE_ADDR(var,oldBase,newBase)	(typeof(var))(((void*)(var) - oldBase) + newBase)

#ifdef __KERNEL__
#define	ALLOC(size)							kmalloc(size,GFP_KERNEL & ~__GFP_WAIT)
#define	FREE(ptr)							kfree(ptr)
#define REALLOC(ptr,size)					krealloc(ptr,size,GFP_KERNEL)
#define STRTOINT(strVar,intVar)				kstrtos32(strVar,10,&intVar)
#define STRTOCHAR(strVar,charVar)			kstrtos8(strVar,10,&charVar)
#define DECLARE_LOCK(varName)				rwlock_t varName
#define DECLARE_LOCK_EXTERN(varName)		extern rwlock_t varName
#define INIT_LOCK(varName)					rwlock_init(&varName)
//#define ACQUIRE_READ_LOCK(varName)			read_lock_irqsave(&varName,flags)
//#define RELEASE_READ_LOCK(varName)			read_unlock_irqrestore(&varName,flags)
#define ACQUIRE_READ_LOCK(varName)			write_lock_irqsave(&varName,flags)
#define RELEASE_READ_LOCK(varName)			write_unlock_irqrestore(&varName,flags)
#define ACQUIRE_WRITE_LOCK(varName)			write_lock_irqsave(&varName,flags)
#define RELEASE_WRITE_LOCK(varName)			write_unlock_irqrestore(&varName,flags)
#else
#define	ALLOC(size)							malloc(size)
#define	FREE(ptr)							free(ptr)
#define REALLOC(ptr,size)					realloc(ptr,size)
#define STRTOINT(strVar,intVar)				(intVar = atoi(strVar))
#define STRTOCHAR(strVar,charVar)			(charVar = atoi(strVar))
#define DECLARE_LOCK(varName)				pthread_mutex_t varName;
#define DECLARE_LOCK_EXTERN(varName)		extern pthread_mutex_t varName;
#define INIT_LOCK(varName)					pthread_mutex_init(&varName,NULL);
#define ACQUIRE_READ_LOCK(varName)			pthread_mutex_lock(&varName);
#define RELEASE_READ_LOCK(varName)			pthread_mutex_unlock(&varName);
#define ACQUIRE_WRITE_LOCK(varName)			pthread_mutex_lock(&varName);
#define RELEASE_WRITE_LOCK(varName)			pthread_mutex_unlock(&varName);
#define USEC_PER_MSEC						1000L
#define TIMER_SIGNAL						SIGRTMIN

#endif

enum {
	ECHILDRENNUM		=	0x1,	// Wrong number of children
	ECHILDRENTYPE,					// One of the children has the wrong type
	ETYPEINFO,						// The typeInfo pointer is not NULL, although it should be or vice versa.
	ECALLBACK,						// A callback pointer is not set
	ERETURNTYPE,					// A source or an event uses a forbidden return type
	ECOMPLEXTYPE,					// A source or an event uses a complex type, which does not exit
	EQUERYSYNTAX,					// A queries syntax is wrong
	EOBECJTIDENT,					// An object uses a forbidden type for its identifier
	EDIFFERENTNODETYPE,				// The new datamodel contains a node with the same as one node in the existings datamodel, but it uses a different type.
	ESAMENODE,						// Both datamodels contains the same node
	ENOMEMORY,						// Out of memory
	EWRONGORDER,					// The investigated operator is not at an allowed position
	ENOELEMENT,						// The provided element does not exist in the datamodel
	ENOPREDICATES,					// No predicates were provided
	ENOELEMENTS,					// No elements were provided
	EWRONGOPERATOR,					// The provided operator is currently not supported
	EUNIT,							// The provided unit is not valid
	ESIZE,							// There was no size or advance parameter provided
	EJOINTYPE,						// Either the joined element does not exist or it has not the correct type
	ENOTCOMPARABLE,					// One or more types used as an operand in a predicate are not comparable. Mostly, this apply for COMPLEX datatypes
	ENOOPERAND,						// At least one operand does not name an element present in the datamodel
	ENOPERIOD,						// No period provided
	ENOOBJSTATUS,					// No valid bitmask for an objects status were provided
	EWRONGSTREAMTYPE,				// The provided stream origin and the corresponding element in the datamodel does not have the same type
	EPARAM,							// At least one parameter has a wrong value
	ERESULTFUNCPTR,					// At least one of the provided queries have no onCompletedFunction pointer set
	EQUERYTYPE,						// 
	EMAXQUERIES						// The maximum number of queries assigned to a node is reached
};

extern void *sharedMemoryBaseAddr;
extern int remainingPages;

#endif // __COMMON_H__
