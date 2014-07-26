#include <resultset.h>

/**
 * Frees the values stored at {@link value}. In order to do this, it searches the datamodel from {@link element} downwards for indirect allocated memory.
 * This could be a string, an array or an string array.
 * @param rootDM a pointer to the slc datamodel
 * @param value a pointer to an instance of {@link element}
 * @param element a pointer to the datamodel element, which describes the layout of the memory {@link value} points to
 */
static void freeItem(DataModelElement_t *rootDM, void *value, DataModelElement_t *element) {
	int i = 0, j = 0, k = 0, type = 0, len = 0, curOffset = 0, prevOffset = 0, steppedDown = 0;
	DataModelElement_t *curNode = NULL, *parentNode = NULL;
	void *curValue = NULL;

	curNode = element;
	curValue = value;
	do {
		steppedDown = 0;
		GET_TYPE_FROM_DM(curNode,type);
		/*
		 * If the current node is a string array, it needs a special treatment.
		 * The code must iterate over all array elements and free them as well.
		 */
		if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
			DEBUG_MSG(2,"Freeing string array: %s@%p, offset=%d\n",curNode->name,(void*)*(PTR_TYPE*)curValue,curOffset);
			len = *(int*)(*((PTR_TYPE*)curValue));
			for (k = 0; k < len; k++) {
				FREE(*(char**)((*(PTR_TYPE*)curValue) + sizeof(int) + k * SIZE_STRING));
			}
			FREE((char*)(*(PTR_TYPE*)curValue));
		} else if (type & ARRAY || type & STRING) {
			// If the element is a string or an array, curValue points to one memory chunk. Therefore, it can be freed with one single call.
			DEBUG_MSG(2,"Freeing array/string: %s@%p, offset=%d\n",curNode->name,(void*)*(PTR_TYPE*)curValue,curOffset);
			FREE((void*)*(PTR_TYPE*)curValue);
		} else if ((type & TYPE) || (type & COMPLEX)) {
			// Oh no. Element is complex datatype. It is necessary to step down and look for further arrays or strings.
			curNode = curNode->children[0];
			prevOffset = 0;
			steppedDown = 1;
		}
		if (steppedDown == 0) {
			// Look for the current nodes sibling.
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
					// Get the offset in bytes within the current struct
					if ((curOffset = getOffset(parentNode,parentNode->children[j]->name)) == -1) {
						return;
					}
					curNode = parentNode->children[j];
					/*
					 * curOffset represents the offset from beginnging of the struct.
					 * Hence, we have to remember the previous offset to just add the margin.
					 */
					curValue = curValue + (curOffset - prevOffset);
					prevOffset = curOffset;
					break;
				}
			// Stop, if the root node is reached.
			};
		}
	} while(curNode != element);
	//FREE(value);
}
/**
 * Frees the tupel, all its items and the memory the items value pointers points to.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to Tupel
 * @see freeItem()
 */
void freeTupel(DataModelElement_t *rootDM, Tupel_t *tupel) {
	DataModelElement_t *element = NULL;
	int i = 0;
	// The tupel is compact. Just one free is needed.
	if (IS_COMPACT(tupel)) {
		FREE(tupel);
		return;
	}
	for (i = 0; i < tupel->itemLen; i++) {
		if (tupel->items[i] == NULL) {
			continue;
		}
		if ((element = getDescription(rootDM,tupel->items[i]->name)) == NULL) {
			//if (tupel->items[i]->value != NULL) {
				// No need to free itmes[i]->value. Interested why? Look at allocItem@resultset.h:361-366
				FREE(tupel->items[i]);
			//}
			continue;
		}
		freeItem(rootDM,tupel->items[i]->value,element);
		DEBUG_MSG(2,"Freeing %s (%p)\n",element->name,tupel->items[i]->value);
		FREE(tupel->items[i]);
	}

	//FREE(tupel->items);
	FREE(tupel);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(freeTupel);
#endif
/**
 * Deletes one item at index {@link slot} from {@link tupel} and frees every memory allocated for it.
 * The item pointer (Tupel_t->items[slot]) is set to NULL. The items array will not be resized.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to Tupel
 * @param slot the index of the item
 */
void deleteItem(DataModelElement_t *rootDM, Tupel_t *tupel, int slot) {
	DataModelElement_t *dm = NULL;
	
	if (!IS_COMPACT(tupel)) {
		dm = getDescription(rootDM,tupel->items[slot]->name);
		freeItem(rootDM,tupel->items[slot]->value,dm);
		FREE(tupel->items[slot]);
	}
	tupel->items[slot] = NULL;
}
/**
 * Calculates the size in bytes of the instance of {@link element} stored at {@link value}.
 * This includes all indirect allocated memory, e.g. arrays, strings ....
 * @param rootDM a pointer to the slc datamodel
 * @param value a pointer to an instance of {@link element}
 * @param element a pointer to the datamodel element, which describes the layout of the memory {@link value} points to
 */
static int getItemSize(DataModelElement_t *rootDM, void *value, DataModelElement_t *element) {
	int i = 0, j = 0, k = 0, type = 0, len = 0, size = 0, temp = 0, curOffset = 0, prevOffset = 0, steppedDown = 0;
	DataModelElement_t *curNode = NULL, *parentNode = NULL;
	void *curValue = NULL;

	curNode = element;
	curValue = value;
	do {
		steppedDown = 0;
		GET_TYPE_FROM_DM(curNode,type);
		//printf("curNode=%s@%d\n",curNode->name,curOffset);
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
			//printf("down: offset=%d\n",curOffset);
		}
		if (steppedDown == 0) {
			// look for the current nodes sibling.
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
					// curValue 
					curValue -= curOffset;
					if ((curOffset = getOffset(curNode->parent,curNode->name)) == -1) {
						return -1;
					}
					prevOffset = curOffset;
					//printf("up: offset=%d\n",curOffset);
				} else {
					// At least one sibling left. Go for it.
					j++;
					// Get the offset in bytes within the current struct
					if ((curOffset = getOffset(parentNode,parentNode->children[j]->name)) == -1) {
						return -1;
					}
					/*
					 * curOffset represents the offset from beginnging of the struct.
					 * Hence, we have to remember the previous offset to just add the margin.
					 */
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
/**
 * Calculates the size in bytes of {@link tupel}, its items, the values and all indirect allocated memory.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to a Tupel_t
 */
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
/**
 * Copies all indirectly used memory for {@link element} to {@link freeMem} and sets the length information and all pointers in {@link newValue} appropriatly.
 * @param rootDM a pointer to the slc datamodel
 * @param oldValue a pointer to the old instance of {@link element}
 * @param newValue a pointer to the new instance of {@link element}
 * @param freeMem a pointer to the remaining free memory
 * @param element a pointer to the datamodel element, which describes the layout of the memory {@link oldValue} points to
 * @return 0, if all memory was successfully copied. -1, otherwise.
 */
static int copyAndCollectAdditionalMem(DataModelElement_t *rootDM, void *oldValue, void *newValue, void *freeMem, DataModelElement_t *element) {
	int i = 0, j = 0, k = 0, type = 0, len = 0, size = 0, temp = 0, curOffset = 0, prevOffset = 0, steppedDown = 0;
	DataModelElement_t *curNode = NULL, *parentNode = NULL;
	void *curValueOld = NULL, *curValueNew = NULL;

	curNode = element;
	curValueOld = oldValue;
	curValueNew = newValue;
	do {
		steppedDown = 0;
		temp = 0;
		GET_TYPE_FROM_DM(curNode,type);
		//printf("curNode=%s@%d\n",curNode->name,curOffset);
		if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
			*((PTR_TYPE*)curValueNew) = (PTR_TYPE)freeMem;
			len = temp = *(int*)(*((PTR_TYPE*)(curValueOld)));
			temp *= SIZE_STRING;
			temp += sizeof(int);
			// First, copy the length information and all pointers.
			memcpy(freeMem,(void*)*((PTR_TYPE*)(curValueOld)),temp);
			freeMem += temp;
			size += temp;
			DEBUG_MSG(2,"Copied string array (name=%s) with %d strings to %p\n",curNode->name,len, (void*)(*(PTR_TYPE*)curValueNew));
			// Second, go across the string array and copy all strings to the new memory area and store a pointer to each string in the pointer array at curValueNew
			for (k = 0; k < len; k++) {
				temp = strlen(*(char**)((*(PTR_TYPE*)curValueOld) + sizeof(int) + k * SIZE_STRING)) + 1;
				*(char**)((*(PTR_TYPE*)curValueNew) + sizeof(int) + k * SIZE_STRING) = freeMem;
				memcpy(*(char**)((*(PTR_TYPE*)curValueNew) + sizeof(int) + k * SIZE_STRING),*(char**)((*(PTR_TYPE*)curValueOld) + sizeof(int) + k * SIZE_STRING),temp);
				DEBUG_MSG(2,"Copied %d string of array (%s) to %p\n",k,curNode->name,*(char**)((*(PTR_TYPE*)curValueNew) + sizeof(int) + k * SIZE_STRING));
				freeMem += temp;
				size += temp;
			}
		} else if (type & ARRAY) {
			*((PTR_TYPE*)curValueNew) = (PTR_TYPE)freeMem;
			len = *(int*)(*((PTR_TYPE*)curValueOld));
			if ((temp = getDataModelSize(rootDM,curNode,1)) == -1) {
				return -1;
			}
			temp = len * temp + sizeof(int);
			// In contrast to a string array this one can be copied in one operation.
			memcpy(freeMem,(void*)*((PTR_TYPE*)curValueOld),temp);
			DEBUG_MSG(2,"Copied an array (%s) with %d elemetns to %p\n",curNode->name,len,(void*)*((PTR_TYPE*)curValueNew));
			size += temp;
			freeMem += temp;
		} else if (type & STRING) {
			*((PTR_TYPE*)curValueNew) = (PTR_TYPE)freeMem;
			temp = strlen((char*)(*(PTR_TYPE*)curValueOld)) + 1;
			memcpy(freeMem,(char*)(*(PTR_TYPE*)curValueOld),temp);
			DEBUG_MSG(2,"Copied string (%s='%s'@%p) to %p with size %d\n",curNode->name,(char*)(*(PTR_TYPE*)curValueOld),(char*)(*(PTR_TYPE*)curValueOld),freeMem,size);
			size += temp;
			freeMem += temp;
		} else if ((type & TYPE) || (type & COMPLEX)) {
			curNode = curNode->children[0];
			prevOffset = 0;
			curOffset = 0;
			steppedDown = 1;
			//printf("down: offset=%d\n",curOffset);
		}
		if (steppedDown == 0) {
			// look for the current nodes sibling.
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
					// curValue 
					curValueNew -= curOffset;
					curValueOld -= curOffset;
					if ((curOffset = getOffset(curNode->parent,curNode->name)) == -1) {
						return -1;
					}
					prevOffset = curOffset;
					//printf("up: offset=%d\n",curOffset);
				} else {
					// At least one sibling left. Go for it.
					j++;
					// Get the offset in bytes within the current struct
					if ((curOffset = getOffset(parentNode,parentNode->children[j]->name)) == -1) {
						return -1;
					}
					/*
					 * curOffset represents the offset from beginnging of the struct.
					 * Hence, we have to remember the previous offset to just add the margin.
					 */
					curNode = parentNode->children[j];
					curValueNew = curValueNew + (curOffset - prevOffset);
					curValueOld = curValueOld + (curOffset - prevOffset);
					prevOffset = curOffset;
					break;
				}
			// Stop, if the root node is reached.
			};
		}
	} while(curNode != element);

	return size;
}
/**
 * Calculates the size in bytes of {@link tupel} and all its indirect memory. Allocates one large chunk.
 * Finally, it copies the tupel, its items, their values and all indirect memory to the new area.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to Tupel
 * @return a pointer to the new tupel on success. NULL otherwise
 * @see copyAndCollectAdditionalMem()
 */
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
	// Mark it as compact and store its size
	ret->isCompact = (size << 8) | 0x1;
	ret->items = (Item_t**)(((void*)ret) + sizeof(Tupel_t));
	// First, count the number of really present items. Due to delete operations one or more items might be deleted.
	for (i = 0; i < tupel->itemLen; i++) {
		if (tupel->items[i] != NULL) {
			numItems++;
		}
	}
	// Second, copy the item poitner array
	memcpy(ret->items,tupel->items,sizeof(Item_t**) * numItems);
	ret->itemLen = numItems;
	newValue = ((void*)ret) + sizeof(Tupel_t) + sizeof(Item_t**) * numItems;
	DEBUG_MSG(2,"Copied tupel to %p and %d item pointers to %p. Starting with value pointer at %p\n",ret,ret->itemLen,ret->items,newValue);

	for (i = 0; i < tupel->itemLen; i++) {
		if (tupel->items[i] == NULL) {
			continue;
		}
		// Third, copy each item
		memcpy(newValue,tupel->items[i],sizeof(Item_t));
		ret->items[j] = newValue;
		// For now, the j-th item has no value.
		ret->items[j]->value = NULL;
		DEBUG_MSG(2,"Copied %d (->%d) item (%s) to %p\n",i,j,ret->items[j]->name,ret->items[j]);
		newValue += sizeof(Item_t);
		// Fourth, get the size of element and copy the directly used memory.
		element = getDescription(rootDM,tupel->items[i]->name);
		size = getDataModelSize(rootDM,element,0);
		memcpy(newValue,tupel->items[i]->value,size);
		ret->items[j]->value = newValue;
		DEBUG_MSG(2,"Copied %d (->%d) items (%s) value bytes (%d) to %p\n",i,j,ret->items[j]->name,size,ret->items[j]->value);
		// Finally, copy all indrectly used memory
		size += copyAndCollectAdditionalMem(rootDM,tupel->items[i]->value,newValue,newValue+size,element);
		DEBUG_MSG(2,"Copied %d additional bytes for item %s\n",size,ret->items[j]->name);
		newValue += size;
		j++;
	}
	
	return ret;
}
