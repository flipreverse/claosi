#ifndef __RESULTSET_H__
#define __RESULTSET_H__

#include <string.h>
#include <stdio.h>
#include <datamodel.h>
#include <query.h>
#include <debug.h>

#define GET_BASE(varName)	(Operator_t*)&varName
#define ADD_PREDICATE(varOperator,slot,predicateVar)	varOperator.predicates[slot] = &predicateVar;
#define ADD_ELEMENT(varOperator,slot,elementVar)	varOperator.elements[slot] = &elementVar;
#define IS_COMPACT(tupelVar)	((tupelVar->isCompact & 0x1) == 0x1)
#define COMPACT_SIZE(tupelVar)	(tupelVar->isCompact >> 8)

#define ALLOC_ITEM_ARRAY(size)	(Item_t**)ALLOC(sizeof(Item_t**) * size)

#define SET_ITEM_INT(tupelVarName,slot,itemValue)	*((int*)tupelVarName->items[slot]->value) = itemValue;
#define SET_ITEM_FLOAT(tupelVarName,slot,itemValue)	*((double*)tupelVarName->items[slot]->value) = itemValue;
#define SET_ITEM_BYTE(tupelVarName,slot,itemValue)	*((char*)tupelVarName->items[slot]->value) = itemValue;
#define SET_ITEM_STRING(tupelVarName,slot,itemValue)	*((int*)tupelVarName->items[slot]->value) = itemValue;

#define GET_MEMBER_POINTER_RETURN(tupelVar,rootDatamodel,typeName,returnValue)	int ret = 0, i = 0; \
DataModelElement_t *dm = NULL; \
char *token = NULL, *tokInput = NULL, *childName = NULL; \
void *valuePtr = NULL; \
for (i = 0; i < tupel->itemLen; i++) { \
	if ((childName = strstr(typeName,tupelVar->items[i]->name)) != NULL) { \
		break; \
	} \
} \
if (i >= tupel->itemLen) { \
	return returnValue; \
} \
valuePtr = tupelVar->items[i]->value; \
childName += strlen(tupelVar->items[i]->name); \
if (strlen(childName) > 0) { \
	if ((dm = getDescription(rootDatamodel,tupelVar->items[i]->name)) == NULL) { \
		return returnValue; \
	} \
	if ((tokInput = ALLOC(strlen(childName) + 1)) == NULL) { \
		return returnValue; \
	} \
	strcpy(tokInput,childName); \
	token = strtok(tokInput,"."); \
	while (token) { \
		if ((ret = getOffset(dm,token)) == -1) { \
			return returnValue; \
		} \
		valuePtr = valuePtr + ret; \
		for(i = 0; i < dm->childrenLen; i++) { \
			if (strcmp(dm->children[i]->name,token) == 0) { \
				break; \
			} \
		} \
		if (i >= dm->childrenLen) { \
			return returnValue; \
		} \
		dm = dm->children[i]; \
		token = strtok(NULL,"."); \
	} \
	FREE(tokInput); \
}

#define GET_MEMBER_POINTER(tupelVar,rootDatamodel,typeName)	int ret = 0, i = 0; \
DataModelElement_t *dm = NULL; \
char *token = NULL, *tokInput = NULL, *childName = NULL; \
void *valuePtr = NULL; \
for (i = 0; i < tupel->itemLen; i++) { \
	if ((childName = strstr(typeName,tupelVar->items[i]->name)) != NULL) { \
		break; \
	} \
} \
if (i >= tupel->itemLen) { \
	return; \
} \
valuePtr = tupelVar->items[i]->value; \
childName += strlen(tupelVar->items[i]->name); \
if ((dm = getDescription(rootDatamodel,tupelVar->items[i]->name)) == NULL) { \
	return; \
} \
if (strlen(childName) > 0) { \
	if ((tokInput = ALLOC(strlen(childName) + 1)) == NULL) { \
		return; \
	} \
	strcpy(tokInput,childName); \
	token = strtok(tokInput,"."); \
	while (token) { \
		if ((ret = getOffset(dm,token)) == -1) { \
			return; \
		} \
		valuePtr = valuePtr + ret; \
		for(i = 0; i < dm->childrenLen; i++) { \
			if (strcmp(dm->children[i]->name,token) == 0) { \
				break; \
			} \
		} \
		if (i >= dm->childrenLen) { \
			return; \
		} \
		dm = dm->children[i]; \
		token = strtok(NULL,"."); \
	} \
	FREE(tokInput); \
}

typedef struct __attribute__((packed)) Item {
	DECLARE_BUFFER(name)
	void *value;
} Item_t;

typedef struct __attribute__((packed)) Tupel {
	unsigned long long timestamp;
	unsigned short itemLen;
	unsigned int isCompact;
	Item_t **items;
} Tupel_t;

static inline void setItemInt(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, int value) {	
	GET_MEMBER_POINTER(tupel,rootDM,typeName);
	*(int*)valuePtr = value;
}

static inline void setItemByte(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, char value) {
	GET_MEMBER_POINTER(tupel,rootDM,typeName);
	*(char*)valuePtr = value;
}

static inline void setItemFloat(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, double value) {
	GET_MEMBER_POINTER(tupel,rootDM,typeName);
	*(double*)valuePtr = value;
}

static inline void setItemString(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, char *value) {
	GET_MEMBER_POINTER(tupel,rootDM,typeName);
	if (IS_COMPACT(tupel)) {
		DEBUG_MSG(1,"Refusing access (%s) to an item, because to tupel is compact.\n",__FUNCTION__);
		return;
	}
	*(PTR_TYPE*)valuePtr = (PTR_TYPE)value;
}

static inline int getItemInt(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName) {
	GET_MEMBER_POINTER_RETURN(tupel,rootDM,typeName,0);
	return *(int*)valuePtr;
}

static inline char getItemByte(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName) {
	GET_MEMBER_POINTER_RETURN(tupel,rootDM,typeName,'\0');
	return *(char*)valuePtr;
}

static inline double getItemFloat(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName) {
	GET_MEMBER_POINTER_RETURN(tupel,rootDM,typeName,0);
	return *(double*)valuePtr;
}

static inline char* getItemString(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName) {
	GET_MEMBER_POINTER_RETURN(tupel,rootDM,typeName,NULL);
	return (char*)*(PTR_TYPE*)valuePtr;
}

static inline void setItemArray(DataModelElement_t *rootDM, Tupel_t *tupel, char *typeName, int num) {
	int size = 0;
	GET_MEMBER_POINTER(tupel,rootDM,typeName);
	size = getSize(rootDM,dm);
	if (IS_COMPACT(tupel)) {
		DEBUG_MSG(1,"Refusing access (%s) to an item, because to tupel is compact.\n",__FUNCTION__);
		return;
	}
	*((PTR_TYPE*)valuePtr) = (PTR_TYPE)ALLOC(num * size + sizeof(int));
	*(int*)(*((PTR_TYPE*)valuePtr)) = num;
	DEBUG_MSG(2,"Allocated %ld@%p bytes for array %s\n",num * size + sizeof(int),(void*)*((PTR_TYPE*)valuePtr),dm->name);
}

static inline void setArraySlotByte(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot,char value) {
	GET_MEMBER_POINTER(tupel,rootDM,arrayTypeName);
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return;
	}
	*(char*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_BYTE) = value;
}

static inline void setArraySlotInt(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot,int value) {
	GET_MEMBER_POINTER(tupel,rootDM,arrayTypeName);
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return;
	}
	*(int*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_INT) = value;
}

static inline void setArraySlotFloat(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot,double value) {
	GET_MEMBER_POINTER(tupel,rootDM,arrayTypeName);
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return;
	}
	*(double*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_FLOAT) = value;
}

static inline void setArraySlotString(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot,char *value) {
	GET_MEMBER_POINTER(tupel,rootDM,arrayTypeName);
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr) || IS_COMPACT(tupel)) {
		return;
	}
	*(char**)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_STRING) = value;
}

static inline char getArraySlotByte(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot) {
	GET_MEMBER_POINTER_RETURN(tupel,rootDM,arrayTypeName,'\0');
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return '\0';
	}
	return *(char*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_BYTE);
}

static inline int getArraySlotInt(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot) {
	GET_MEMBER_POINTER_RETURN(tupel,rootDM,arrayTypeName,0);
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return 0;
	}
	return *(int*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_INT);
}

static inline double getArraySlotFloat(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot) {
	GET_MEMBER_POINTER_RETURN(tupel,rootDM,arrayTypeName,0);
	return *(double*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_FLOAT);
}

static inline char* getArraySlotString(DataModelElement_t *rootDM,Tupel_t *tupel,char *arrayTypeName,int arraySlot) {
	GET_MEMBER_POINTER_RETURN(tupel,rootDM,arrayTypeName,NULL);
	if (arraySlot >= *(int*)(*(PTR_TYPE*)valuePtr)) {
		return NULL;
	}
	return (char*)((*(PTR_TYPE*)(valuePtr)) + sizeof(int) + arraySlot * SIZE_STRING);
}

static inline int initTupel(Tupel_t **tupel,unsigned long long timestamp, int numItems) {
	if ((*tupel = (Tupel_t*)ALLOC(sizeof(Tupel_t) + numItems * sizeof(Item_t**))) == NULL) {
		return -1;
	}
	(*tupel)->isCompact = 0;
	(*tupel)->timestamp = timestamp;
	(*tupel)->itemLen = numItems;
	(*tupel)->items = (Item_t**)(*tupel + 1);
	return 0;
}

static inline int allocItem(DataModelElement_t *rootDM, Tupel_t *tupel, int slot, char *itemTypeName) {
	DataModelElement_t *dm = NULL;
	int ret = 0;

	if (IS_COMPACT(tupel)) {
		DEBUG_MSG(1,"Refusing access (%s) to an item, because to tupel is compact.\n",__FUNCTION__);
		return -1;
	}
	if ((dm = getDescription(rootDM,itemTypeName)) == NULL) {
		return -1;
	}
	if ((ret = getSize(rootDM,dm)) == -1) {
		return -1;
	}
	if ((tupel->items[slot] = ALLOC(sizeof(Item_t))) == NULL) {
		return -1;
	}
	tupel->items[slot]->value = ALLOC(ret);
	strncpy((char*)&tupel->items[slot]->name,itemTypeName,MAX_NAME_LEN);
	DEBUG_MSG(2,"Allocated %d@%p bytes for %s\n",ret,tupel->items[slot]->value,dm->name);
	return 0;
}

static inline int addItem(Tupel_t **tupel, int newItems) {
	Tupel_t *temp = NULL;
	
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
Tupel_t* copyAndCollectTupel(DataModelElement_t *rootDM, Tupel_t *tupel);

#endif // __RESULTSET_H__
