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
#include <linux/delay.h>
#else
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/queue.h>
#define PAGE_SIZE 4096
#endif

#define EVALUATION
//#undef EVALUATION

#define MAX_NAME_LEN						40
#define DECLARE_BUFFER(name)				char name[MAX_NAME_LEN + 1];
#define PROCFS_DIR_NAME						"slc"
#define PROCFS_LOCKFILE						"lock"
#define PROCFS_COMMFILE						"comm"
#define SLC_DATA_MODEL						(slcDataModel)
#define REWRITE_ADDR(var,oldBase,newBase)	(typeof(var))(((void*)(var) - oldBase) + newBase)
#define TEST_BIT(varName,bit)				(((varName) & bit) == bit)
#define SET_BIT(varName,bit)				((varName) |= bit)
#define CLEAR_BIT(varName,bit)				((varName) = (varName) & ~(1 << bit))

#ifdef __KERNEL__
#define	ALLOC(size)							kmalloc(size,GFP_KERNEL & ~__GFP_WAIT)
#define	FREE(ptr)							kfree(ptr)
#define REALLOC(ptr,size)					krealloc(ptr,size,GFP_KERNEL & ~__GFP_WAIT)
#define STRTOINT(strVar,intVar)				kstrtos32(strVar,10,&intVar)
#define STRTOCHAR(strVar,charVar)			kstrtos8(strVar,10,&charVar)
#define DECLARE_LOCK(varName)				rwlock_t varName
#define DECLARE_LOCK_EXTERN(varName)		extern rwlock_t varName
#define INIT_LOCK(varName)					rwlock_init(&varName)
#define ACQUIRE_READ_LOCK(varName)			read_lock_irqsave(&varName,flags)
#define TRY_READ_LOCK(varName)				read_trylock(&varName)
#define RELEASE_READ_LOCK(varName)			read_unlock_irqrestore(&varName,flags)
#define ACQUIRE_WRITE_LOCK(varName)			write_lock_irqsave(&varName,flags)
#define RELEASE_WRITE_LOCK(varName)			write_unlock_irqrestore(&varName,flags)
#define MSLEEP(x)							mdelay(x)
#define LAYER_CODE							0x1
#define ENDPOINT_CONNECTED()				(atomic_read(&communicationFileMmapRef) >= 1)
extern atomic_t communicationFileMmapRef;

/**
 * Declares a list named <varNamePrefix>QueriesList which is intended to store a query registered to a source, event or object.
 * Furthermore, it declares a lock named <varNamePrefix>ListLock which is used to protect the list against concurrent access.
 */
#define DECLARE_QUERY_LIST(varNamePrefix) static LIST_HEAD(varNamePrefix ## QueriesList); \
static DEFINE_SPINLOCK(varNamePrefix ## ListLock);
/**
 * Acquires the read lock on slcLockVar and <varNamePrefix>ListLock.
 * Iterates over every entry in <varNamePrefix>QueriesList. The current element will be stored in tempListVar.
 * It's intended to use in conjunction with events.
 */
#define forEachQueryEvent(slcLockVar, varNamePrefix, tempListVar, tempVar)		ACQUIRE_READ_LOCK(slcLockVar); \
	spin_lock(&varNamePrefix ## ListLock); \
	list_for_each(tempListVar,&varNamePrefix ## QueriesList) { \
	tempVar = container_of(tempListVar,QuerySelectors_t,list);
/**
 * Releases the <varNamePrefix>ListLock and the slcLockVar
 */
#define endForEachQueryEvent(slcLockVar,varNamePrefix)				} \
	spin_unlock(&varNamePrefix ## ListLock); \
	RELEASE_READ_LOCK(slcLockVar);
/**
 * Acquires the read lock on slcLockVar and <varNamePrefix>ListLock.
 * Iterates over every entry in <varNamePrefix>QueriesList. The current element will be stored in tempListVar.
 * It's intended to use in conjunction with objects. Therefore it has an additional parameter newEvent. Each query
 * in the list which does not registered for newEvent will be skipped.
 */
#define forEachQueryObject(slcLockVar, varNamePrefix, tempListVar, tempVar, newEvent)	ACQUIRE_READ_LOCK(slcLockVar); \
	spin_lock(&varNamePrefix ## ListLock); \
	list_for_each(tempListVar,&varNamePrefix ## QueriesList) { \
	tempVar = container_of(tempListVar,QuerySelectors_t,list); \
	if ((((ObjectStream_t*)tempVar->query->root)->objectEvents & newEvent) != newEvent) { \
		continue; \
	}
/**
 * Releases the <varNamePrefix>ListLock and the slcLockVar
 */
#define endForEachQueryObject(slcLockVar,varNamePrefix)				} \
	spin_unlock(&varNamePrefix ## ListLock); \
	RELEASE_READ_LOCK(slcLockVar);
/**
 * Allocates a QuerySeletor_t, assigns the query (queryVar) to it, acquires <varNamePrefix>ListLock and inserts 
 * it in the <varNamePrefix>QueriesList.
 * listEmptyVar will be 1, if the list was empty before insertion.
 */
#define addAndEnqueueQuery(varNamePrefix,listEmptyVar, tempVar, queryVar) tempVar = (QuerySelectors_t*)ALLOC(sizeof(QuerySelectors_t)); \
	if (tempVar == NULL) { \
		return; \
	} \
	listEmptyVar = list_empty(&varNamePrefix ## QueriesList); \
	tempVar->query = queryVar; \
	spin_lock(&varNamePrefix ## ListLock); \
	list_add_tail(&tempVar->list,&varNamePrefix ## QueriesList); \
	spin_unlock(&varNamePrefix ## ListLock);
/**
 * Searches the <varNamePrefix>QueriesList for an element which query member is equal to queryVar.
 * If it was found, it will be safely removed from list while holding the <varNamePrefix>ListLock.
 * listEmptyVar will be 1, if the list is empty after removal.
 */
#define findAndDeleteQuery(varNamePrefix,listEmptyVar, tempVar, queryVar, listPos, listNext)	spin_lock(&varNamePrefix ## ListLock); \
	list_for_each_safe(listPos,listNext,&varNamePrefix ## QueriesList) { \
		tempVar = container_of(listPos,QuerySelectors_t,list); \
		if (tempVar->query == query) { \
			list_del(&tempVar->list); \
			FREE(tempVar); \
			break; \
		} \
	} \
	listEmptyVar = list_empty(&varNamePrefix ## QueriesList); \
	spin_unlock(&varNamePrefix ## ListLock);

#else
#define	ALLOC(size)							malloc(size)
#define	FREE(ptr)							free(ptr)
#define REALLOC(ptr,size)					realloc(ptr,size)
#define STRTOINT(strVar,intVar)				(intVar = atoi(strVar))
#define STRTOCHAR(strVar,charVar)			(charVar = atoi(strVar))
#define DECLARE_LOCK(varName)				pthread_rwlock_t varName
#define DECLARE_LOCK_EXTERN(varName)		extern pthread_rwlock_t varName
#define INIT_LOCK(varName)					pthread_rwlock_init(&varName,NULL)
#define ACQUIRE_READ_LOCK(varName)			pthread_rwlock_wrlock(&varName)
#define TRY_READ_LOCK(varName)				pthread_rwlock_tryrdlock(&varName)
#define RELEASE_READ_LOCK(varName)			pthread_rwlock_unlock(&varName)
#define ACQUIRE_WRITE_LOCK(varName)			pthread_rwlock_rdlock(&varName)
#define RELEASE_WRITE_LOCK(varName)			pthread_rwlock_unlock(&varName)
#define USEC_PER_MSEC						1000L
#define USEC_PER_SEC						1000000L
#define TIMER_SIGNAL						SIGRTMIN
#define MSLEEP(x)							usleep((x) * 1000)
#define LAYER_CODE							0x2
#define ENDPOINT_CONNECTED()				(1)

#define DECLARE_QUERY_LIST(varNamePrefix) static LIST_HEAD(varNamePrefix ## QueriesListHEAD,QuerySelectors) varNamePrefix ## QueriesList = LIST_HEAD_INITIALIZER(varNamePrefix ## QueriesList); \
static pthread_mutex_t varNamePrefix ## ListLock;

#define forEachQueryEvent(slcLockVar, varNamePrefix, tempListVar, tempVar)		ACQUIRE_READ_LOCK(slcLockVar); \
	pthread_mutex_lock(&varNamePrefix ## ListLock); \
	LIST_FOREACH(tempListVar,&varNamePrefix ## QueriesList,listEntry) { 

#define endForEachQueryEvent(slcLockVar,varNamePrefix)				} \
	pthread_mutex_unlock(&varNamePrefix ## ListLock); \
	RELEASE_READ_LOCK(slcLockVar);

#define forEachQueryObject(slcLockVar, varNamePrefix, tempListVar, tempVar, newEvent)	ACQUIRE_READ_LOCK(slcLockVar); \
	pthread_mutex_lock(&varNamePrefix ## ListLock); \
	LIST_FOREACH(tempListVar,&varNamePrefix ## QueriesList,listEntry) { \
	if ((((ObjectStream_t*)tempVar->query->root)->objectEvents & newEvent) != newEvent) { \
		continue; \
	}

#define endForEachQueryObject(slcLockVar,varNamePrefix)				} \
	pthread_mutex_unlock(&varNamePrefix ## ListLock); \
	RELEASE_READ_LOCK(slcLockVar);

#define addAndEnqueueQuery(varNamePrefix,listEmptyVar, tempVar, queryVar) tempVar = (QuerySelectors_t*)ALLOC(sizeof(QuerySelectors_t)); \
	if (tempVar == NULL) { \
		return; \
	} \
	listEmptyVar = LIST_EMPTY(&varNamePrefix ## QueriesList); \
	tempVar->query = queryVar; \
	pthread_mutex_lock(&varNamePrefix ## ListLock); \
	LIST_INSERT_HEAD(&varNamePrefix ## QueriesList,tempVar,listEntry); \
	pthread_mutex_unlock(&varNamePrefix ## ListLock);

#define findAndDeleteQuery(varNamePrefix,listEmptyVar, tempVar, queryVar, listNext)	pthread_mutex_lock(&varNamePrefix ## ListLock); \
	for (tempVar = LIST_FIRST(&varNamePrefix ## QueriesList); tempVar != NULL; tempVar = listNext) { \
		listNext = LIST_NEXT(tempVar,listEntry); \
		if (tempVar->query == query) { \
			LIST_REMOVE(tempVar,listEntry); \
			FREE(tempVar); \
			break; \
		} \
	} \
	listEmptyVar = LIST_EMPTY(&varNamePrefix ## QueriesList); \
	pthread_mutex_unlock(&varNamePrefix ## ListLock);


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
	EMAXQUERIES,					// The maximum number of queries assigned to a node is reached
	ESELECTORS						//
};

static inline unsigned long long getCycles(void) {
	unsigned long long ret = 0;

#if defined(__i386__)
	unsigned int low = 0, high = 0;
	asm volatile("CPUID\n\t"
				 "RDTSC\n\t"
				 "mov %%edx, %0\n\t"
				 "mov %%eax, %1\n\t"
				 : "=r" (high), "=r" (low) :
				 : "%eax", "%edx");
	ret = ((unsigned long long)high << 32) | (unsigned long long)low;
#elif defined(__x86_64__)
	unsigned int low = 0, high = 0;
	asm volatile("CPUID\n\t"
				 "RDTSC\n\t"
				 "mov %%edx, %0\n\t"
				 "mov %%eax, %1\n\t"
				 : "=r" (high), "=r" (low):
				 : "%rax", "%rbx", "%rcx", "%rdx");
	ret = ((unsigned long long)high << 32) | (unsigned long long)low;
#elif defined(__arm__)
	ret = 0;
#else
#error Unknown architecture
#endif

	return ret;
}

#endif // __COMMON_H__
