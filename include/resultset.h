#ifndef __RESULTSET_H__
#define __RESULTSET_H__

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif
#include <output.h>
#include <datamodel.h>

DECLARE_LOCK_EXTERN(slcLock);

enum TupleFlags {
	TUPLE_COMPACT		=	0x1
};

#define ALLOC_ITEM_ARRAY(size)	(Item_t**)ALLOC(sizeof(Item_t**) * size)

/**
 * This macro does the same as GET_MEMBER_POINTER_ALGO_ONLY besides it declares
 * the datamodelElementVar and the valuePtrVar.
 * This macro is used in all get and set methods.
 */
/**
 * Sometimes a path specifitcations leads to a Source, Object or an Event.
 * If so, it is necessary to resolve the actual type by interpreting the type-specific information stored in
 * typeInfo.
 */
#define GET_TYPE_FROM_DM(varName,varType) if (varName->dataModelType == SOURCE) { \
	varType = ((Source_t*)varName->typeInfo)->returnType; \
} else if (varName->dataModelType == EVENT) { \
	varType = ((Event_t*)varName->typeInfo)->returnType; \
} else if (varName->dataModelType == OBJECT) { \
	varType = ((Object_t*)varName->typeInfo)->identifierType; \
} else { \
	varType = varName->dataModelType; \
}

typedef struct __attribute__((packed)) Item {
	DECLARE_BUFFER(name)						// A path describing the way down to the datamodel element stored at value, e.g. "net.packetType" and value points to the first element of packetType.
	void *value;								// A pointer to a memory area where the values resides
} Item_t;

typedef struct __attribute__((packed)) Tupel {
	struct Tupel *next;
	unsigned long long timestamp;				// The current time since 1-1-1970 in ms
	unsigned short itemLen;						// Number of items
	unsigned int flags;						// If the first byte contains an one, the tupel and its items are stored in one large memory area. If so, the remaining bytes contain the size of thie area in bytes.
	Item_t **items;
} Tupel_t;

/**
 * This algorithm is the basis for each operation on a tupel.
 * First, it tries to find an item which name (tupelVar->items[i]->name) matches the first part of the provided element name.
 * This element name is provided by a user as a string (typeName) containing the path to the node in the datamodel.
 * If the algorithm finds a suitable item, it strips the first part (childName += strlen(tupelVar->items[i]->name);) off. Otherwise it will abort.
 * If the remaining name is not empty, the algorithm will tokenize the string and walk down the datamodel. In addition to that, the pointer
 * to the memory area, where the values resides, will be set as well during the walk down.
 * After terminating the datamodelElementVar variable will hold a pointer to a DataModelElement_t* describing the element and the valuePtrVar points to
 * the memory area, where its value resides.
 * 
 * @param rootDM a pointer to the root of the datamodel
 * @param tuple a pointer to the tuple which should be examined
 * @param typeName a string describing the path from the root down to the node whose value we want to get
 * @param dm this function might store a pointer to the datamodel node found, or NULL. the argument might be NULL.
 */
static inline void* getMemberPointer(DataModelElement_t *rootDM, Tupel_t *tuple, char *typeName, DataModelElement_t **dm) {
	DataModelElement_t *tempDM = NULL;
	void *valuePtr = NULL;
	int ret = 0, i = 0;
	char *token = NULL, *tokInput = NULL, *tokInput_ = NULL, *childName = typeName;

	if (dm != NULL) {
		*dm = NULL;
	}
	if (tuple == NULL) {
		return NULL;
	}
	ret = -1;
	for (i = 0; i < tuple->itemLen; i++) {
		if (tuple->items[i] == NULL) {
			continue;
		}
		if (strcmp(typeName,tuple->items[i]->name) == 0) {
			ret = i;
			break;
		} else {
			if (strstr(typeName,tuple->items[i]->name) != NULL) {
				ret = i;
			}
		}
	}
	if (ret == -1) {
		return NULL;
	}

	valuePtr = tuple->items[ret]->value;
	childName += strlen(tuple->items[ret]->name);
	tempDM = getDescription(rootDM,tuple->items[ret]->name);
	if (tempDM == NULL) {
		return NULL;
	}
	if (strlen(childName) > 0) {
		if (tempDM->dataModelType != TYPE) {
			return NULL;
		}
		if (*childName == '.') {
			childName++;
		}
		tokInput = ALLOC(strlen(childName) + 1);
		if (tokInput == NULL) {
			return NULL;
		}
		tokInput_ = tokInput;
		strcpy(tokInput,childName);
		token = strsep(&tokInput,".");
		while (token) {
			ret = getOffset(tempDM,token);
			if (ret == -1) {
				FREE(tokInput_);
				return NULL;
			}
			valuePtr = valuePtr + ret;
			for(i = 0; i < tempDM->childrenLen; i++) {
				if (strcmp(tempDM->children[i]->name,token) == 0) {
					break;
				}
			}
			if (i >= tempDM->childrenLen) {
				FREE(tokInput_);
				return NULL;
			}
			tempDM = tempDM->children[i];
			token = strsep(&tokInput,".");
		}
		FREE(tokInput_);
	}

	if (dm != NULL) {
		*dm = tempDM;
	}
	return valuePtr;
}

/**
 * The following functions should help to access a tupel.
 * They can be used to set values within a flat memory area. Flat means that the datamodel does not contain
 * any array, but it is allowed to have nested structs, e.g. packetType { int first; complext second; }.
 * The typeName parameter describes the path to the desired element, e.g. "packetType.second".
 * Arrays are just allowed as an endpoint of a path.
 */
static inline void setItemInt(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, int value) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,NULL);
	if (valuePtr == NULL) {
		return;
	}
	*(int*)valuePtr = value;
}

static inline void setItemByte(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, char value) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,NULL);
	if (valuePtr == NULL) {
		return;
	}
	*(char*)valuePtr = value;
}

static inline void setItemFloat(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, double value) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,NULL);
	if (valuePtr == NULL) {
		return;
	}
	*(double*)valuePtr = value;
}

static inline void setItemString(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, char *value) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,NULL);
	if (valuePtr == NULL) {
		PRINT_MSG("LOL\n");
		return;
	}
	if (TEST_BIT(tupel->flags,TUPLE_COMPACT)) {
		DEBUG_MSG(1,"Refusing access (%s) to an item, because tuple is compact.\n",__FUNCTION__);
		return;
	}
	*(PTR_TYPE*)valuePtr = (PTR_TYPE)value;
}
/**
 * Allocates an array with {@link num} elements at {@link typeName}.
 * If the tupel is compact, the function refueses access to the tupel, because its sized is fixed.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to the tupel
 * @param typeName a path specification to describe the wy through the datamodel
 * @param num array size
 */
static inline void setItemArray(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, int num) {
	int size = 0;
	void *valuePtr = NULL;
	DataModelElement_t *dm = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,&dm);
	if (valuePtr == NULL) {
		return;
	}
	size = getSize(rootDM,dm);
	if (TEST_BIT(tupel->flags,TUPLE_COMPACT)) {
		DEBUG_MSG(1,"Refusing access (%s) to an item, because tupel is compact.\n",__FUNCTION__);
		return;
	}
	if ((*((PTR_TYPE*)valuePtr) = (PTR_TYPE)ALLOC(num * size + sizeof(int))) == 0) {
		DEBUG_MSG(1,"Cannot allocate array: %s\n",typeName);
		return;
	}
	*(int*)(*((PTR_TYPE*)valuePtr)) = num;
	DEBUG_MSG(2,"Allocated %ld@%p bytes for array %s\n",(long)(num * size + sizeof(int)),(void*)*((PTR_TYPE*)valuePtr),dm->name);
}
/**
 * The following functions should help to access an array inside a tupel.
 * They can be used to set values within a flat memory area. Flat means that the datamodel does not contain
 * any array, but it is allowed to have nested structs, e.g. packetType { int first; complext second; }.
 * The typeName parameter describes the path to the desired element, e.g. "packetType.second".
 * Arrays are just allowed as an endpoint of a path.
 */
static inline void setArraySlotByte(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot,char value) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return;
	}
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return;
	}
	*(char*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_BYTE) = value;
}

static inline void setArraySlotInt(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot,int value) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return;
	}
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return;
	}
	*(int*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_INT) = value;
}

static inline void setArraySlotFloat(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot,double value) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return;
	}
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return;
	}
	*(double*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_FLOAT) = value;
}

static inline void setArraySlotString(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot,char *value) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return;
	}
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr) || TEST_BIT(tupel->flags,TUPLE_COMPACT)) {
		return;
	}
	*(char**)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_STRING) = value;
}
/**
 * Copies the array {@link valueArray} with {@link n} elements to the array described by{@link arrayTypeName}. Starting at {@link startingSlot}.
 * If {@link n} is larger than the array size, just array size - {@link startingSlot} bytes will be copied from {@link valueArrays}.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to the tupel
 * @param arrayTypeName a path specification to describe the wy through the datamodel
 * @param startingSlot the first element in the target array that should be written
 * @param valueArray a pointer to an array where the values will be copied from
 */
static inline void copyArrayByte(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int startingSlot, char *valueArray, int n) {
	int size = 0, toCopy = 0, i = 0;
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return;
	}
	size = *(int*)(*(PTR_TYPE*)valuePtr);
	if (startingSlot >= size || valueArray == NULL) {
		return;
	}
	toCopy = ((size - startingSlot) > n ? n : (size - startingSlot));
	for (i = 0; i < toCopy; i++) {
		*(char*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + (startingSlot + i) * SIZE_BYTE) = valueArray[i];
	}
}

static inline int getItemInt(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,NULL);
	if (valuePtr == NULL) {
		return -1;
	}
	return *(int*)valuePtr;
}

static inline char getItemByte(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,NULL);
	if (valuePtr == NULL) {
		return '\0';
	}
	return *(char*)valuePtr;
}
#ifndef __KERNEL__
static inline double getItemFloat(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,NULL);
	if (valuePtr == NULL) {
		return -1;
	}
	return *(double*)valuePtr;
}
#endif
static inline char* getItemString(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,typeName,NULL);
	if (valuePtr == NULL) {
		return NULL;
	}
	return (char*)*(PTR_TYPE*)valuePtr;
}

static inline char getArraySlotByte(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return '\0';
	}
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return '\0';
	}
	return *(char*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_BYTE);
}

static inline int getArraySlotInt(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return -1;
	}
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return 0;
	}
	return *(int*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_INT);
}
#ifndef __KERNEL__
static inline double getArraySlotFloat(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return -1;
	}
	return *(double*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_FLOAT);
}
#endif
static inline char* getArraySlotString(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot) {
	void *valuePtr = NULL;
	valuePtr = getMemberPointer(rootDM,tupel,arrayTypeName,NULL);
	if (valuePtr == NULL) {
		return NULL;
	}
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return NULL;
	}
	return (char*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_STRING);
}
/**
 * Allocates a tupel and the item pointer array. It initializes the tupel and item pointer arrray.
 * @param timestamp time in millisecond the tupel was created
 * @param numItmes the number of items
 * @return a pointer to a newly allocated tupel or NULL if allocation fails.
 */
static inline Tupel_t* initTupel(unsigned long long timestamp, int numItems) {
	int i = 0;
	Tupel_t *ret = NULL;
	if ((ret = (Tupel_t*)ALLOC(sizeof(Tupel_t) + numItems * sizeof(Item_t*))) == NULL) {
		return NULL;
	}
	ret->flags = 0;
	ret->next = NULL;
	ret->timestamp = timestamp;
	ret->itemLen = numItems;
	ret->items = (Item_t**)(ret + 1);
	for (i = 0; i < numItems; i++) {
		ret->items[i] = NULL;
	}
	return ret;
}
/**
 * Allocates a new item and assigns its pointer to the {@link tupel}. Its name will be set to {@link itemTypeName}.
 * Furthermore, {@link itemTypeName} is used to determine the number of bytes allocated and assigned to the items value pointer.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to the tupel
 * @param slot the position within the item pointer array
 * @param itemTypeName a path description for the datatype which should be allocated
 * @return 0 on success. -1 otherwise.
 */
static inline int allocItem(DataModelElement_t *rootDM, Tupel_t *tupel, int slot, char *itemTypeName) {
	DataModelElement_t *dm = NULL;
	char *mem = NULL;
	int ret = 0;

	if (TEST_BIT(tupel->flags,TUPLE_COMPACT)) {
		DEBUG_MSG(1,"Refusing access (%s) to an item, because to tupel is compact.\n",__FUNCTION__);
		return -1;
	}
	if ((dm = getDescription(rootDM,itemTypeName)) == NULL) {
		return -1;
	}
	if ((ret = getSize(rootDM,dm)) == -1) {
		return -1;
	}
	if ((mem = ALLOC(sizeof(Item_t) + ret)) == NULL) {
		return -1;
	}
	tupel->items[slot] = (Item_t*)mem;
	tupel->items[slot]->value = mem + sizeof(Item_t);
	strncpy((char*)&tupel->items[slot]->name,itemTypeName,MAX_NAME_LEN);
	DEBUG_MSG(2,"Allocated %d@%p bytes for %s\n",ret,tupel->items[slot]->value,dm->name);
	return 0;
}

/**
 * Grows the tupel by {@link newItems} items. {@link tupel} is a pointer pointer, because a realloc may allocate a new
 * memory area and copy the contents to the new location.
 * @param tupel a pointer to the tupel pointer
 * @param newItems number of new items the tupel will be grown by
 * @return 0 on success. -1 otherwise.
 */
static inline int addItem(Tupel_t **tupel, int newItems) {
	Tupel_t *temp = NULL;
	
	if (TEST_BIT((*tupel)->flags,TUPLE_COMPACT)) {
		DEBUG_MSG(1,"Refusing access (%s) to an item, because to tupel is compact.\n",__FUNCTION__);
		return -1;
	}
	if ((temp = REALLOC(*tupel,sizeof(Tupel_t) + sizeof(Item_t**) * ((*tupel)->itemLen + newItems))) == NULL) {
		return -1;
	}
	*tupel = temp;
	(*tupel)->itemLen += newItems;
	(*tupel)->items = (Item_t**)(*tupel + 1);
	return 0;
}

void printTupel(DataModelElement_t *rootDM, Tupel_t *tupel);
void freeTupel(DataModelElement_t *rootDM, Tupel_t *tupel);
int getTupelSize(DataModelElement_t *rootDM, Tupel_t *tupel);
int copyAndCollectTupel(DataModelElement_t *rootDM, Tupel_t *tupel, void *freeMem, int tupleSize);
void deleteItem(DataModelElement_t *rootDM, Tupel_t *tupel, int slot);
Tupel_t* copyTupel(DataModelElement_t *rootDM, Tupel_t *tuple);
void rewriteTupleAddress(DataModelElement_t *rootDM, Tupel_t *tuple, void *oldBaseAddr, void *newBaseAddr);
int mergeTuple(DataModelElement_t *rootDM, Tupel_t **tupleA, Tupel_t *tupleB);

#endif // __RESULTSET_H__
