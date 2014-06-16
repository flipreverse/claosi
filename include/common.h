#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef __KERNEL__
#include <linux/slab.h>
#include <linux/string.h>
#else
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#endif

#define MAX_NAME_LEN						40
#define DECLARE_BUFFER(name)				char name[MAX_NAME_LEN + 1];

#ifdef __KERNEL__
#define PRINT_MSG(args...)					printk(KERN_INFO args);
#define	ALLOC(size)							kmalloc(size,GFP_KERNEL)
#define	FREE(ptr)							kfree(ptr)
#define REALLOC(ptr,size)					krealloc(ptr,size,GFP_KERNEL)
#define STRTOINT(strVar,intVar)				kstrtos32(strVar,10,&intVar)
#define STRTOCHAR(strVar,charVar)			kstrtos8(strVar,10,&charVar)
#else
#define PRINT_MSG(args...)					printf(args);
#define	ALLOC(size)							malloc(size)
#define	FREE(ptr)							free(ptr)
#define REALLOC(ptr,size)					realloc(ptr,size)
#define STRTOINT(strVar,intVar)				(intVar = atoi(strVar))
#define STRTOCHAR(strVar,charVar)			(charVar = atoi(strVar))
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
	ENOFERQ,						// No frequency provided
	ENOOBJSTATUS,					// No valid bitmask for an objects status were provided
	EWRONGSTREAMTYPE,				// The provided stream origin and the corresponding element in the datamodel does not have the same type
	EPARAM,							// At least one parameter has a wrong value
	ERESULTFUNCPTR,					// At least one of the provided queries have no onCompletedFunction pointer set
	EQUERYTYPE,						// 
	EMAXQUERIES						// The maximum number of queries assigned to a node is reached
};

#endif // __COMMON_H__
