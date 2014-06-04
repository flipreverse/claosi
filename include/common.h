#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdlib.h>

#define MAX_NAME_LEN	40
#define DECLARE_BUFFER(name)	char name[MAX_NAME_LEN + 1];

#define	ALLOC(size)							malloc(size)
#define	FREE(ptr)							free(ptr)

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
	EWRONGSTREAMTYPE				// The provided stream origin and the corresponding element in the datamodel does not have the same type
};

#endif // __COMMON_H__
