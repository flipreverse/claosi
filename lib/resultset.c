#include <resultset.h>

static void freeItem(DataModelElement_t *rootDM, void *value, DataModelElement_t *element) {
	int i = 0, j = 0, k = 0, type = 0, len = 0, curOffset = 0, prevOffset = 0, steppedDown = 0;
	DataModelElement_t *curNode = NULL, *parentNode = NULL;
	void *curValue = NULL;

	curNode = element;
	curValue = value;
	do {
		steppedDown = 0;
		GET_TYPE_FROM_DM(curNode,type);
		if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
			DEBUG_MSG(2,"Freeing string array: %s@%p, offset=%d\n",curNode->name,(void*)*(PTR_TYPE*)curValue,curOffset);
			len = *(int*)(*((PTR_TYPE*)curValue));
			for (k = 0; k < len; k++) {
				FREE(*(char**)((*(PTR_TYPE*)curValue) + sizeof(int) + k * SIZE_STRING));
			}
			FREE((char*)(*(PTR_TYPE*)curValue));
		} else if (type & ARRAY || type & STRING) {
			DEBUG_MSG(2,"Freeing array/string: %s@%p, offset=%d\n",curNode->name,(void*)*(PTR_TYPE*)curValue,curOffset);
			FREE((void*)*(PTR_TYPE*)curValue);
		} else if ((type & TYPE) || (type & COMPLEX)) {
			curNode = curNode->children[0];
			prevOffset = 0;
			steppedDown = 1;
		}
		if (steppedDown == 0) {
			// Second, look for the current nodes sibling.
			while(curNode != element) {
				j = -1;
				parentNode = curNode->parent;
				for (i = 0; i < parentNode->childrenLen; i++) {
					if (parentNode->children[i] == curNode) {
						j = i;
						break;
					}
				}
				if (j == parentNode->childrenLen - 1) {
					// If there is none, go one level up and look for this nodes sibling.
					curNode = parentNode;
					curValue -= curOffset;
					if ((curOffset = getOffset(curNode->parent,curNode->name)) == -1) {
						return;
					}
					prevOffset = curOffset;
				} else {
					// At least one sibling left. Go for it.
					j++;
					if ((curOffset = getOffset(parentNode,parentNode->children[j]->name)) == -1) {
						return;
					}
					curNode = parentNode->children[j];
					curValue = curValue + (curOffset - prevOffset);
					prevOffset = curOffset;
					break;
				}
			// Stop, if the root node is reached.
			};
		}
	} while(curNode != element);
	FREE(value);
}

void freeTupel(DataModelElement_t *rootDM, Tupel_t *tupel) {
	DataModelElement_t *element = NULL;
	int i = 0;
	
	if (IS_COMPACT(tupel)) {
		FREE(tupel);
		return;
	}
	for (i = 0; i < tupel->itemLen; i++) {
		if (tupel->items[i] == NULL) {
			continue;
		}
		if ((element = getDescription(rootDM,tupel->items[i]->name)) == NULL) {
			if (tupel->items[i]->value != NULL) {
				FREE(tupel->items[i]->value);
			}
			continue;
		}
		freeItem(rootDM,tupel->items[i]->value,element);
		DEBUG_MSG(2,"Freeing %s (%p)\n",element->name,tupel->items[i]->value);
		FREE(tupel->items[i]);
	}

	//FREE(tupel->items);
	FREE(tupel);
}

void deleteItem(DataModelElement_t *rootDM, Tupel_t *tupel, int slot) {
	DataModelElement_t *dm = NULL;
	
	if (!IS_COMPACT(tupel)) {
		dm = getDescription(rootDM,tupel->items[slot]->name);
		freeItem(rootDM,tupel->items[slot]->value,dm);
		FREE(tupel->items[slot]);
	}
	tupel->items[slot] = NULL;
}

static int getItemSize(DataModelElement_t *rootDM, void *value, DataModelElement_t *element) {
	int i = 0, j = 0, k = 0, type = 0, len = 0, size = 0, temp = 0, curOffset = 0, prevOffset = 0, steppedDown = 0;
	DataModelElement_t *curNode = NULL, *parentNode = NULL;
	void *curValue = NULL;

	curNode = element;
	curValue = value;
	do {
		steppedDown = 0;
		GET_TYPE_FROM_DM(curNode,type);
		printf("curNode=%s@%d\n",curNode->name,curOffset);
		if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
			DEBUG_MSG(2,"Calculating size of string array %s@%p (%d)\n",curNode->name,(void*)*(PTR_TYPE*)curValue,size);
			len = *(int*)(*((PTR_TYPE*)curValue));
			temp = len * sizeof(char*) + sizeof(int);
			for (k = 0; k < len; k++) {
				temp += strlen(*(char**)((*(PTR_TYPE*)curValue) + sizeof(int) + k * SIZE_STRING)) + 1;
			}
			size += temp;
			DEBUG_MSG(2,"Calculated size of string array %s@%p (%d)\n",curNode->name,(void*)*(PTR_TYPE*)curValue,size);
		} else if (type & ARRAY) {
			DEBUG_MSG(2,"Calculating size of array %s@%p (%d)\n",curNode->name,(void*)*(PTR_TYPE*)curValue,size);
			len = *(int*)(*((PTR_TYPE*)curValue));
			if ((temp = getDataModelSize(rootDM,curNode,1)) == -1) {
				return -1;
			}
			size += len * temp + sizeof(int);
			DEBUG_MSG(2,"Calculated size of array %s@%p (%d)\n",curNode->name,(void*)*(PTR_TYPE*)curValue,size);
		} else if (type & STRING) {
			DEBUG_MSG(2,"Calculating size of string %s@%p (%d)\n",curNode->name,(void*)*(PTR_TYPE*)curValue,size);
			size += strlen((char*)(*(PTR_TYPE*)curValue)) + 1;
			DEBUG_MSG(2,"Calculated size of string %s@%p (%d)\n",curNode->name,(void*)*(PTR_TYPE*)curValue,size);
		} else if ((type & TYPE) || (type & COMPLEX)) {
			curNode = curNode->children[0];
			prevOffset = 0;
			curOffset = 0;
			steppedDown = 1;
					printf("down: offset=%d\n",curOffset);
		}
		if (steppedDown == 0) {
			// Second, look for the current nodes sibling.
			while(curNode != element) {
				j = -1;
				parentNode = curNode->parent;
				for (i = 0; i < parentNode->childrenLen; i++) {
					if (parentNode->children[i] == curNode) {
						j = i;
						break;
					}
				}
				if (j == parentNode->childrenLen - 1) {
					// If there is none, go one level up and look for this nodes sibling.
					curNode = parentNode;
					curValue -= curOffset;
					if ((curOffset = getOffset(curNode->parent,curNode->name)) == -1) {
						return -1;
					}
					prevOffset = curOffset;
					printf("up: offset=%d\n",curOffset);
				} else {
					// At least one sibling left. Go for it.
					j++;
					if ((curOffset = getOffset(parentNode,parentNode->children[j]->name)) == -1) {
						return -1;
					}
					curNode = parentNode->children[j];
					curValue = curValue + (curOffset - prevOffset);
					prevOffset = curOffset;
					break;
				}
			// Stop, if the root node is reached.
			};
		}
	} while(curNode != element);

	return size;
}



int getTupelSize(DataModelElement_t *rootDM, Tupel_t *tupel) {
	DataModelElement_t *element = NULL;
	int i = 0, ret = 0, size = 0;
	
	size = sizeof(Tupel_t);
	for (i = 0; i < tupel->itemLen; i++) {
		if (tupel->items[i] == NULL) {
			continue;
		}
		if ((element = getDescription(rootDM,tupel->items[i]->name)) == NULL) {
			return -1;
		}
		if ((ret = getDataModelSize(rootDM,element,0)) == -1) {
			return -1;
		}
		size += ret;
		if ((ret = getItemSize(rootDM,tupel->items[i]->value,element)) == -1) {
			return -1;
		}
		size += ret + sizeof(Item_t) + sizeof(Item_t**);
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
	
	GET_TYPE_FROM_DM(element,type);
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
	int size = 0, i = 0, j = 0, numItems = 0;
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
	for (i = 0; i < tupel->itemLen; i++) {
		if (tupel->items[i] != NULL) {
			numItems++;
		}
	}
	memcpy(ret->items,tupel->items,sizeof(Item_t**) * numItems);
	ret->itemLen = numItems;
	newValue = ((void*)ret) + sizeof(Tupel_t) + sizeof(Item_t**) * numItems;
	DEBUG_MSG(2,"Copied tupel to %p and %d item pointers to %p. Starting with value pointer at %p\n",ret,ret->itemLen,ret->items,newValue);

	for (i = 0; i < tupel->itemLen; i++) {
		if (tupel->items[i] == NULL) {
			continue;
		}
		memcpy(newValue,tupel->items[i],sizeof(Item_t));
		ret->items[j] = newValue;
		ret->items[j]->value = NULL;
		DEBUG_MSG(2,"Copied %d (->%d) item (%s) to %p\n",i,j,ret->items[j]->name,ret->items[j]);
		newValue += sizeof(Item_t);
		
		element = getDescription(rootDM,tupel->items[i]->name);
		size = getDataModelSize(rootDM,element,0);
		memcpy(newValue,tupel->items[i]->value,size);
		ret->items[j]->value = newValue;
		DEBUG_MSG(1,"Copied %d (->%d) items (%s) value bytes (%d) to %p\n",i,j,ret->items[j]->name,size,ret->items[j]->value);
	
		size += copyAndCollectAdditionalMem(rootDM,tupel->items[i]->value,newValue,newValue+size,element);
		newValue += size;
		j++;
	}
	
	return ret;
}
