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
		} else if (type & COMPLEX) {
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
					if ((curOffset = getOffset(rootDM,curNode->parent,curNode->name)) == -1) {
						return;
					}
					prevOffset = curOffset;
				} else {
					// At least one sibling left. Go for it.
					j++;
					// Get the offset in bytes within the current struct
					if ((curOffset = getOffset(rootDM,parentNode,parentNode->children[j]->name)) == -1) {
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
	if (TEST_BIT(tupel->flags,TUPLE_COMPACT)) {
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
		DEBUG_MSG(2,"Freeing %s (%p)\n",element->name,tupel->items[i]->value);
		freeItem(rootDM,tupel->items[i]->value,element);
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
	
	if (!TEST_BIT(tupel->flags,TUPLE_COMPACT)) {
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
		} else if (type & COMPLEX) {
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
					if ((curOffset = getOffset(rootDM,curNode->parent,curNode->name)) == -1) {
						return -1;
					}
					prevOffset = curOffset;
					//printf("up: offset=%d\n",curOffset);
				} else {
					// At least one sibling left. Go for it.
					j++;
					// Get the offset in bytes within the current struct
					if ((curOffset = getOffset(rootDM,parentNode,parentNode->children[j]->name)) == -1) {
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
		} else if (type & COMPLEX) {
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
					if ((curOffset = getOffset(rootDM,curNode->parent,curNode->name)) == -1) {
						return -1;
					}
					prevOffset = curOffset;
					//printf("up: offset=%d\n",curOffset);
				} else {
					// At least one sibling left. Go for it.
					j++;
					// Get the offset in bytes within the current struct
					if ((curOffset = getOffset(rootDM,parentNode,parentNode->children[j]->name)) == -1) {
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
 * Copies the tupel, its items, their values and all indirect memory to the new area.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to Tupel
 * @param freeMem a pointer 
 * @param tupleSize the size of the allocated memory chunk which corresponds to the size of the tuple
 * @return the number of bytes used to copy the tuple to {@link freeMem}.
 * @see copyAndCollectAdditionalMem()
 */
int copyAndCollectTupel(DataModelElement_t *rootDM, Tupel_t *tupel, void *freeMem, int tupleSize) {
	int size = 0, i = 0, j = 0, numItems = 0;
	Tupel_t *ret = NULL;
	DataModelElement_t *element = NULL;
	void *newValue = NULL;

	if (freeMem == NULL) {
		return -1;
	}
	ret = (Tupel_t*)freeMem;
	memcpy(ret,tupel,sizeof(Tupel_t));
	// Mark it as compact and store its size
	SET_BIT(ret->flags,TUPLE_COMPACT);
	ret->next = NULL;
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
	return newValue - freeMem;
}
/**
 * Copies all indirectly used memory for {@link element} and sets the length information and all pointers in {@link newValue} appropriatly.
 * @param rootDM a pointer to the slc datamodel
 * @param oldValue a pointer to the old instance of {@link element}
 * @param newValue a pointer to the new instance of {@link element}
 * @param element a pointer to the datamodel element, which describes the layout of the memory {@link oldValue} points to
 * @return 0, if all memory was successfully copied. -1, otherwise.
 */
static int copyAdditionalMem(DataModelElement_t *rootDM, void *oldValue, void *newValue, DataModelElement_t *element) {
	int i = 0, j = 0, k = 0, type = 0, len = 0, temp = 0, curOffset = 0, prevOffset = 0, steppedDown = 0;
	DataModelElement_t *curNode = NULL, *parentNode = NULL;
	void *curValueOld = NULL, *curValueNew = NULL, *ret = NULL;

	curNode = element;
	curValueOld = oldValue;
	curValueNew = newValue;
	do {
		steppedDown = 0;
		temp = 0;
		GET_TYPE_FROM_DM(curNode,type);
		//printf("curNode=%s@%d\n",curNode->name,curOffset);
		if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
			len = temp = *(int*)(*((PTR_TYPE*)(curValueOld)));
			temp *= SIZE_STRING;
			temp += sizeof(int);
			ret = ALLOC(temp);
			if (ret == NULL) {
				return -1;
			}
			*((PTR_TYPE*)curValueNew) = (PTR_TYPE)ret;
			// First, copy the length information and all pointers.
			memcpy(ret,(void*)*((PTR_TYPE*)(curValueOld)),temp);
			DEBUG_MSG(2,"Copied string array (name=%s) with %d strings to %p\n",curNode->name,len, (void*)(*(PTR_TYPE*)curValueNew));
			// Second, go across the string array and copy all strings to the new memory area and store a pointer to each string in the pointer array at curValueNew
			for (k = 0; k < len; k++) {
				temp = strlen(*(char**)((*(PTR_TYPE*)curValueOld) + sizeof(int) + k * SIZE_STRING)) + 1;
				ret = ALLOC(temp);
				if (ret == NULL) {
					return -1;
				}
				*(char**)((*(PTR_TYPE*)curValueNew) + sizeof(int) + k * SIZE_STRING) = ret;
				memcpy(*(char**)((*(PTR_TYPE*)curValueNew) + sizeof(int) + k * SIZE_STRING),*(char**)((*(PTR_TYPE*)curValueOld) + sizeof(int) + k * SIZE_STRING),temp);
				DEBUG_MSG(2,"Copied %d string of array (%s) to %p\n",k,curNode->name,*(char**)((*(PTR_TYPE*)curValueNew) + sizeof(int) + k * SIZE_STRING));
			}
		} else if (type & ARRAY) {
			len = *(int*)(*((PTR_TYPE*)curValueOld));
			if ((temp = getDataModelSize(rootDM,curNode,1)) == -1) {
				return -1;
			}
			temp = len * temp + sizeof(int);
			ret = ALLOC(temp);
			if (ret == NULL) {
				return -1;
			}
			*((PTR_TYPE*)curValueNew) = (PTR_TYPE)ret;
			// In contrast to a string array this one can be copied in one operation.
			memcpy(ret,(void*)*((PTR_TYPE*)curValueOld),temp);
			DEBUG_MSG(2,"Copied an array (%s) with %d elemetns to %p\n",curNode->name,len,(void*)*((PTR_TYPE*)curValueNew));
		} else if (type & STRING) {
			temp = strlen((char*)(*(PTR_TYPE*)curValueOld)) + 1;
			ret = ALLOC(temp);
			if (ret == NULL) {
				return -1;
			}
			*((PTR_TYPE*)curValueNew) = (PTR_TYPE)ret;
			memcpy(ret,(char*)(*(PTR_TYPE*)curValueOld),temp);
			DEBUG_MSG(2,"Copied string (%s='%s'@%p) to %p with size %d\n",curNode->name,(char*)(*(PTR_TYPE*)curValueOld),(char*)(*(PTR_TYPE*)curValueOld),ret,temp);
		} else if (type & COMPLEX) {
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
					if ((curOffset = getOffset(rootDM,curNode->parent,curNode->name)) == -1) {
						return -1;
					}
					prevOffset = curOffset;
					//printf("up: offset=%d\n",curOffset);
				} else {
					// At least one sibling left. Go for it.
					j++;
					// Get the offset in bytes within the current struct
					if ((curOffset = getOffset(rootDM,parentNode,parentNode->children[j]->name)) == -1) {
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

	return 0;
}
/**
 * Copies a tuple, its items and all indirect used memory. The necessary memory will be allocated dynamically.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to Tupel
 * @return a pointer to the new tupel on success. NULL otherwise
 * @see copyAdditionalMem()
 */
Tupel_t* copyTupel(DataModelElement_t *rootDM, Tupel_t *tuple) {
	int size = 0, i = 0, j = 0, numItems = 0;
	Tupel_t *ret = NULL;
	DataModelElement_t *element = NULL;

	// Count the number of present items
	for (i = 0; i < tuple->itemLen; i++) {
		if (tuple->items[i] != NULL) {
			numItems++;
		}
	}
	// Allocate and init the copy tuple
	ret = initTupel(tuple->timestamp,numItems);
	if (ret == NULL) {
		return NULL;
	}

	for (i = 0; i < tuple->itemLen; i++) {
		if (tuple->items[i] == NULL) {
			continue;
		}
		element = getDescription(rootDM,tuple->items[i]->name);
		size = getDataModelSize(rootDM,element,0);
		// Allocate memory for the item as well for the value
		ret->items[j] = ALLOC(sizeof(Item_t) + size);
		if (ret->items[j] == NULL) {
			freeTupel(rootDM,ret);
			return NULL;
		}
		memcpy(ret->items[j],tuple->items[i],sizeof(Item_t));
		ret->items[j]->value = ret->items[j] + 1;
		DEBUG_MSG(2,"Copied %d (->%d) item (%s) to %p\n",i,j,ret->items[j]->name,ret->items[j]);
		// Copy the items value
		memcpy(ret->items[j]->value,tuple->items[i]->value,size);
		DEBUG_MSG(2,"Copied %d (->%d) items (%s) value bytes (%d) to %p\n",i,j,ret->items[j]->name,size,ret->items[j]->value);
		// Finally, copy all indrectly used memory
		if (copyAdditionalMem(rootDM,tuple->items[i]->value,ret->items[j]->value,element) == -1) {
			freeTupel(rootDM,ret);
			return NULL;
		}
		DEBUG_MSG(2,"Copied additional bytes for item %s\n",ret->items[j]->name);
		j++;
	}

	return ret;
}
/**
 * Depending on the memory layout which is derived from the {@link element} the function traverses all indirect allocated memory and rewrites all
 * addresses.
 * @param rootDM a pointer to the slc datamodel
 * @param valuePtr a pointer to the memory location where an instance of {@link element} is stored
 * @param oldBaseAddr a pointer to the old base address
 * @param newBaseAddr a pointer to the new base address
 * @param element a pointer to the datamodel element, which describes the layout of the memory {@link valuePtr} points to
 */
static int rewriteAdditionalMem(DataModelElement_t *rootDM, void *valuePtr, void *oldBaseAddr, void *newBaseAddr, DataModelElement_t *element) {
	int i = 0, j = 0, k = 0, type = 0, len = 0, curOffset = 0, prevOffset = 0, steppedDown = 0;
	DataModelElement_t *curNode = NULL, *parentNode = NULL;
	void *curValue = NULL;
	char **temp = NULL;
	PTR_TYPE *ptr = NULL;

	curNode = element;
	curValue = valuePtr;

	do {
		steppedDown = 0;
		temp = 0;
		GET_TYPE_FROM_DM(curNode,type);
		if ((type & (STRING | ARRAY)) == (STRING | ARRAY)) {
			ptr = (PTR_TYPE*)(curValue);
			*ptr = REWRITE_ADDR(*ptr,oldBaseAddr,newBaseAddr);
			len = *(int*)(*((PTR_TYPE*)(curValue)));
			DEBUG_MSG(2,"Rewriting addresses of a string array (name=%s) with %d strings to %p\n",curNode->name,len, (void*)(*(PTR_TYPE*)curValue));
			// Second, go across the string array and copy all strings to the new memory area and store a pointer to each string in the pointer array at curValueNew
			for (k = 0; k < len; k++) {
				temp = (char**)((*(PTR_TYPE*)curValue) + sizeof(int) + k * SIZE_STRING);
				*temp = REWRITE_ADDR(*temp,oldBaseAddr,newBaseAddr);
				DEBUG_MSG(2,"Rewrote address of %d. string of array (%s) to %p\n",k,curNode->name,*(char**)((*(PTR_TYPE*)curValue) + sizeof(int) + k * SIZE_STRING));
			}
		} else if (type & ARRAY) {
			ptr = (PTR_TYPE*)(curValue);
			*ptr = REWRITE_ADDR(*ptr,oldBaseAddr,newBaseAddr);
			DEBUG_MSG(2,"Rewrote array (%s) with %d elemetns at %p\n",curNode->name,len,(void*)*((PTR_TYPE*)curValue));
		} else if (type & STRING) {
			ptr = (PTR_TYPE*)(curValue);
			*ptr = REWRITE_ADDR(*ptr,oldBaseAddr,newBaseAddr);
			DEBUG_MSG(2,"Rewrote string (%s='%s'@%p)\n",curNode->name,(char*)(*(PTR_TYPE*)curValue),(char*)(*(PTR_TYPE*)curValue));
		} else if (type & COMPLEX) {
			curNode = curNode->children[0];
			prevOffset = 0;
			curOffset = 0;
			steppedDown = 1;
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
					if ((curOffset = getOffset(rootDM,curNode->parent,curNode->name)) == -1) {
						return -1;
					}
					prevOffset = curOffset;
					//printf("up: offset=%d\n",curOffset);
				} else {
					// At least one sibling left. Go for it.
					j++;
					// Get the offset in bytes within the current struct
					if ((curOffset = getOffset(rootDM,parentNode,parentNode->children[j]->name)) == -1) {
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

	return 0;
}
/**
 * Traverses the tuple starting at {@link tuple} and rewrites each pointer in the item array and each pointer to indirect allocated memory.
 * It calculates the offset of each object according to {@link oldBaseAddr}, calculates the new absolute address according to {@link newBaseAddr}
 * and writes it back.
 * @param rootDM a pointer to the slc datamodel
 * @param tupel a pointer to Tupel
 * @param oldBaseAddr The base address of the old memory area
 * @param newBaseAddr The base address of the new memory area
 */
void rewriteTupleAddress(DataModelElement_t *rootDM, Tupel_t *tuple, void *oldBaseAddr, void *newBaseAddr) {
	int i = 0;
	DataModelElement_t *element = NULL;

	if (tuple->next != NULL) {
		tuple->next = REWRITE_ADDR(tuple->next,oldBaseAddr,newBaseAddr);
	}
	if (tuple->itemLen > 0) {
		tuple->items = REWRITE_ADDR(tuple->items,oldBaseAddr,newBaseAddr);
		for (i = 0; i < tuple->itemLen; i++) {
			// Skip empty items
			if (tuple->items[i] == NULL) {
				continue;
			}
			tuple->items[i] = REWRITE_ADDR(tuple->items[i],oldBaseAddr,newBaseAddr);
			tuple->items[i]->value = REWRITE_ADDR(tuple->items[i]->value,oldBaseAddr,newBaseAddr);
			element = getDescription(rootDM,tuple->items[i]->name);
			rewriteAdditionalMem(rootDM,tuple->items[i]->value,oldBaseAddr,newBaseAddr,element);
		}
	}
}
/**
 * Merges all items of {@link tupleB} into {@link tupleA}. Already present items are skipped.
 * All needless memory will be freed. Hence, {@link tupleB} will contain an invalid (!!) address after completing the merge.
 * The address may change due to relocation and resizing of the items array of {@link tuplA}.
 * @param a pointer to the root of the datamodel
 * @param tupleA a pointer pointer to the tuple where {@link tupleB}s items will be stored
 * @param tupleB a pointer to the tuple which items should be merged into {@link tupleA}
 * @return 0 on success. -1 otherwise.
 */
int mergeTuple(DataModelElement_t *rootDM, Tupel_t **tupleA, Tupel_t *tupleB) {
	int i = 0, j = 0, newItems = 0, newIdx = 0, deleted = 0;
	DataModelElement_t *element = NULL;

	// count the number of mergeable items
	for (i = 0; i < tupleB->itemLen; i++) {
		deleted = 0;
		for (j = 0; j < (*tupleA)->itemLen; j++) {
			// Already present?
			if (strcmp(tupleB->items[i]->name,(*tupleA)->items[j]->name) == 0) {
				DEBUG_MSG(2,"Item (%s) already present\n",tupleB->items[i]->name);
				element = getDescription(rootDM,tupleB->items[i]->name);
				if (element == NULL) {
					// No need to free itmes[i]->value. Interested why? Look at allocItem@resultset.h:361-366
					FREE(tupleB->items[i]);
					break;
				}
				// Yes! Delete it.
				DEBUG_MSG(2,"Freeing %s (%p)\n",element->name,tupleB->items[i]->value);
				freeItem(rootDM,tupleB->items[i]->value,element);
				FREE(tupleB->items[i]);
				tupleB->items[i] = NULL;
				deleted = 1;
				break;
			}
		}
		if (deleted == 0) {
			newItems++;
		}
	}
	DEBUG_MSG(2,"Adding %d new items.\n",newItems);
	newIdx = (*tupleA)->itemLen;
	if (addItem(tupleA,newItems) == -1) {
		return -1;
	}
	for (i = 0; i < tupleB->itemLen; i++) {
		if (tupleB->items[i] == NULL) {
			DEBUG_MSG(2,"Skipping tupleB->items[%d]\n",i);
			continue;
		}
		// Just reassign the pointer to an item. No further allocation needed.
		(*tupleA)->items[newIdx] = tupleB->items[i];
		newIdx++;
	}
	FREE(tupleB);

	return 0;
}
