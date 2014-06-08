#include <resultset.h>

static void freeItem(DataModelElement_t *rootDM, void *value, DataModelElement_t *element);

static void freeAdditionalItemSpace(DataModelElement_t *rootDM, void *value, DataModelElement_t *element) {
	int j = 0, k = 0, len = 0, offset = 0;

	for (j = 0; j < element->childrenLen; j++) {
		if ((offset = getOffset(element,element->children[j]->name)) == -1) {
			continue;
		}
		if ((element->children[j]->dataModelType & (STRING | ARRAY)) == (STRING | ARRAY)) {
			DEBUG_MSG(2,"Freeing string array: %s@%p, offset=%d\n",element->children[j]->name,(void*)*(PTR_TYPE*)(value+offset),offset);
			len = *(int*)(*((PTR_TYPE*)(value + offset)));
			for (k = 0; k < len; k++) {
				FREE(*(char**)((*(PTR_TYPE*)(value + offset)) + sizeof(int) + k * SIZE_STRING));
			}
			FREE((char*)(*(PTR_TYPE*)(value + offset)));
		} else if (element->children[j]->dataModelType & ARRAY || element->children[j]->dataModelType & STRING) {
			DEBUG_MSG(2,"Freeing array/string: %s@%p, offset=%d\n",element->children[j]->name,(void*)*(PTR_TYPE*)(value + offset),offset);
			FREE((void*)*(PTR_TYPE*)(value + offset));
		} else if ((element->children[j]->dataModelType & TYPE) || (element->children[j]->dataModelType & COMPLEX)) {
			freeItem(rootDM,value + offset,element->children[j]);
		}
	}
}

static void freeItem(DataModelElement_t *rootDM, void *value, DataModelElement_t *element) {
	int j = 0, k = 0, len = 0,type = 0;
	
	if (element->dataModelType == SOURCE) {
		type = ((Source_t*)element->typeInfo)->returnType;
	} else if (element->dataModelType == EVENT) {
		type = ((Event_t*)element->typeInfo)->returnType;
	} else if (element->dataModelType == OBJECT) {
		type = ((Object_t*)element->typeInfo)->identifierType;
	} else {
		type = element->dataModelType;
	}

	if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
		DEBUG_MSG(1,"Freeing string array: %s (=%p)\n",element->name,(void*)*(PTR_TYPE*)value);
		len = *(int*)(*((PTR_TYPE*)value));
		for (k = 0; k < len; k++) {
			FREE(*(char**)((*(PTR_TYPE*)value) + sizeof(int) + k * SIZE_STRING));
		}
		FREE((void*)(*(PTR_TYPE*)value));
	} else if (type & ARRAY || type & STRING) {
		DEBUG_MSG(2,"Freeing array/string: %s (=%p)\n",element->children[j]->name,(void*)*(PTR_TYPE*)value);
		FREE((void*)*(PTR_TYPE*)value);
	} else if ((type & TYPE) || (type & COMPLEX)) {
		freeAdditionalItemSpace(rootDM,value,element);
	}
}

void freeTupel(DataModelElement_t *rootDM, Tupel_t *tupel) {
	DataModelElement_t *element = NULL;
	int i = 0;
	
	if (IS_COMPACT(tupel)) {
		FREE(tupel);
		return;
	}
	for (i = 0; i < tupel->itemLen; i++) {
		if ((element = getDescription(rootDM,tupel->items[i]->name)) == NULL) {
			if (tupel->items[i]->value != NULL) {
				FREE(tupel->items[i]->value);
			}
			continue;
		}
		freeItem(rootDM,tupel->items[i]->value,element);
		DEBUG_MSG(1,"Freeing %s (%p)\n",element->name,tupel->items[i]->value);
		FREE(tupel->items[i]->value);
		FREE(tupel->items[i]);
	}

	//FREE(tupel->items);
	FREE(tupel);
}

static int getAddiotionalItemSize(DataModelElement_t *rootDM, void *value, DataModelElement_t *element);

static int getIndirectSize(DataModelElement_t *rootDM, void *value, DataModelElement_t *element) {
	int j = 0, offset = 0, size = 0, ret = -1;

	for (j = 0; j < element->childrenLen; j++) {
		if ((offset = getOffset(element,element->children[j]->name)) == -1) {
			return -1;
		}
		if ((ret = getAddiotionalItemSize(rootDM,value + offset, element->children[j])) == -1) {
			return -1;
		}
		size += ret;
	}
	
	return size;
}

static int getAddiotionalItemSize(DataModelElement_t *rootDM, void *value, DataModelElement_t *element) {
	int k = 0, len = 0, size = 0, ret = 0, type = 0;
	
	if (element->dataModelType == SOURCE) {
		type = ((Source_t*)element->typeInfo)->returnType;
	} else if (element->dataModelType == EVENT) {
		type = ((Event_t*)element->typeInfo)->returnType;
	} else if (element->dataModelType == OBJECT) {
		type = ((Object_t*)element->typeInfo)->identifierType;
	} else {
		type = element->dataModelType;
	}

	if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
		DEBUG_MSG(2,"Calculating size of string array %s@%p (%d)\n",element->name,(void*)*(PTR_TYPE*)value,size);
		len = *(int*)(*((PTR_TYPE*)(value)));
		size += len * sizeof(char*) + sizeof(int);
		for (k = 0; k < len; k++) {
			size += strlen(*(char**)((*(PTR_TYPE*)value) + sizeof(int) + k * SIZE_STRING)) + 1;
		}
		DEBUG_MSG(2,"Calculated size of string array %s@%p (%d)\n",element->name,(void*)*(PTR_TYPE*)value,size);
	} else if (type & ARRAY) {
		DEBUG_MSG(2,"Calculating size of array %s@%p (%d)\n",element->name,(void*)*(PTR_TYPE*)value,size);
		len = *(int*)(*((PTR_TYPE*)value));
		if ((ret = getDataModelSize(rootDM,element,1)) == -1) {
			return -1;
		}
		size += len * ret + sizeof(int);
		DEBUG_MSG(2,"Calculated size of array %s@%p (%d)\n",element->name,(void*)*(PTR_TYPE*)value,size);
	} else if (type & STRING) {
		DEBUG_MSG(2,"Calculating size of string %s@%p (%d)\n",element->name,(void*)*(PTR_TYPE*)value,size);
		size += strlen((char*)(*(PTR_TYPE*)value)) + 1;
		DEBUG_MSG(2,"Calculated size of string %s@%p (%d)\n",element->name,(void*)*(PTR_TYPE*)value,size);
	} else if ((type & TYPE) || (type & COMPLEX)) {
		if ((ret = getIndirectSize(rootDM,value,element)) == -1) {
			return -1;
		}
		size += ret;
	}
	return size;
}

int getTupelSize(DataModelElement_t *rootDM, Tupel_t *tupel) {
	DataModelElement_t *element = NULL;
	int i = 0, ret = 0, size = 0;
	
	size = sizeof(Tupel_t);
	size += sizeof(Item_t**) * tupel->itemLen;
	size += sizeof(Item_t) * tupel->itemLen;
	for (i = 0; i < tupel->itemLen; i++) {
		if ((element = getDescription(rootDM,tupel->items[i]->name)) == NULL) {
			return -1;
		}
		if ((ret = getDataModelSize(rootDM,element,0)) == -1) {
			return -1;
		}
		size += ret;
		if ((ret = getAddiotionalItemSize(rootDM,tupel->items[i]->value,element)) == -1) {
			return -1;
		}
		size += ret;
	}	
	
	return size;
}

static int copyAndCollectAdditionalMem(DataModelElement_t *rootDM, void *oldValue, void *newValue, void *freeMem, DataModelElement_t *element);

static int copyAndCollectIndirectMem(DataModelElement_t *rootDM, void *oldValue, void *newValue, void *freeMem, DataModelElement_t *element) {
	int j = 0, offset = 0, size = 0;

	for (j = 0; j < element->childrenLen; j++) {
		offset = getOffset(element,element->children[j]->name);
		size += copyAndCollectAdditionalMem(rootDM,oldValue + offset, newValue + offset,freeMem,element->children[j]);
	}
	
	return size;
}

static int copyAndCollectAdditionalMem(DataModelElement_t *rootDM, void *oldValue, void *newValue, void *freeMem, DataModelElement_t *element) {
	int k = 0, len = 0, size = 0, ret = 0, type = 0;
	
	if (element->dataModelType == SOURCE) {
		type = ((Source_t*)element->typeInfo)->returnType;
	} else if (element->dataModelType == EVENT) {
		type = ((Event_t*)element->typeInfo)->returnType;
	} else if (element->dataModelType == OBJECT) {
		type = ((Object_t*)element->typeInfo)->identifierType;
	} else {
		type = element->dataModelType;
	}
	if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
		*((PTR_TYPE*)newValue) = (PTR_TYPE)freeMem;
		len = size = *(int*)(*((PTR_TYPE*)(oldValue)));
		size *= SIZE_STRING;
		size += sizeof(int);
		memcpy(freeMem,(void*)*((PTR_TYPE*)(oldValue)),size);
		freeMem += size;
		DEBUG_MSG(2,"Copied string array (name=%s) with %d strings to %p\n",element->name,len, (void*)(*(PTR_TYPE*)newValue));
		
		for (k = 0; k < len; k++) {
			ret = strlen(*(char**)((*(PTR_TYPE*)oldValue) + sizeof(int) + k * SIZE_STRING)) + 1;
			*(char**)((*(PTR_TYPE*)newValue) + sizeof(int) + k * SIZE_STRING) = freeMem;
			memcpy(*(char**)((*(PTR_TYPE*)newValue) + sizeof(int) + k * SIZE_STRING),*(char**)((*(PTR_TYPE*)oldValue) + sizeof(int) + k * SIZE_STRING),ret);
			DEBUG_MSG(2,"Copied %d string of array (%s) to %p\n",k,element->name,*(char**)((*(PTR_TYPE*)newValue) + sizeof(int) + k * SIZE_STRING));
			freeMem += ret;
			size += ret;
		}
	} else if (type & ARRAY) {
		*((PTR_TYPE*)newValue) = (PTR_TYPE)freeMem;
		len = *(int*)(*((PTR_TYPE*)oldValue));
		if ((ret = getDataModelSize(rootDM,element,1)) == -1) {
			return -1;
		}
		size = len * ret + sizeof(int);
		memcpy(freeMem,(void*)*((PTR_TYPE*)oldValue),size);
		DEBUG_MSG(2,"Copied an array (%s) with %d elemetns to %p, %p\n",element->name,len,(void*)*((PTR_TYPE*)newValue),newValue);
	} else if (type & STRING) {
		*((PTR_TYPE*)newValue) = (PTR_TYPE)freeMem;
		size = strlen((char*)(*(PTR_TYPE*)oldValue)) + 1;
		memcpy(freeMem,(char*)(*(PTR_TYPE*)oldValue),size);
		DEBUG_MSG(2,"Copied string (%s='%s'@%p) to %p with size %d\n",element->name,(char*)(*(PTR_TYPE*)oldValue),(char*)(*(PTR_TYPE*)oldValue),freeMem,size);
	} else if ((type & TYPE) || (type & COMPLEX)) {
		size = copyAndCollectIndirectMem(rootDM,oldValue,newValue,freeMem,element);
	}
	return size;
}

Tupel_t* copyAndCollectTupel(DataModelElement_t *rootDM, Tupel_t *tupel) {
	int size = 0, i = 0;
	Tupel_t *ret = NULL;
	DataModelElement_t *element = NULL;
	void *newValue = NULL;
	
	if ((size = getTupelSize(rootDM,tupel)) == -1) {
		return NULL;
	}
	if ((ret = ALLOC(size)) == NULL) {
		return NULL;
	}
	memcpy(ret,tupel,sizeof(Tupel_t));
	ret->isCompact = (size << 8) | 0x1;
	ret->items = (Item_t**)(((void*)ret) + sizeof(Tupel_t));
	memcpy(ret->items,tupel->items,sizeof(Item_t**) * tupel->itemLen);
	newValue = ((void*)ret) + sizeof(Tupel_t) + sizeof(Item_t**) * tupel->itemLen;
	DEBUG_MSG(2,"Copied tupel to %p and %d item pointers to %p. Starting with value pointer at %p\n",ret,ret->itemLen,ret->items,newValue);
	
	
	for (i = 0; i < tupel->itemLen; i++) {
		memcpy(newValue,tupel->items[i],sizeof(Item_t));
		ret->items[i] = newValue;
		ret->items[i]->value = NULL;
		DEBUG_MSG(2,"Copied %d item (%s) to %p\n",i,ret->items[i]->name,ret->items[i]);
		newValue += sizeof(Item_t);
		
		element = getDescription(rootDM,tupel->items[i]->name);
		size = getDataModelSize(rootDM,element,0);
		memcpy(newValue,tupel->items[i]->value,size);
		ret->items[i]->value = newValue;
		DEBUG_MSG(2,"Copied %d items (%s) value bytes (%d) to %p\n",i,ret->items[i]->name,size,ret->items[i]->value);
	
		size += copyAndCollectAdditionalMem(rootDM,tupel->items[i]->value,newValue,newValue+size,element);
		newValue += size;
	}
	
	return ret;
}
