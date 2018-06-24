#include <common.h>
#include <datamodel.h>
#include <communication.h>
#include <liballoc.h>

/**
 * Tries to resolve an element described by {@link name} to an instance of DataModelElement_t.
 * @param root The root of a datamodel.
 * @param name Contains a path to the desired element. Each element is separated by a dot.
 * @return A pointer to a DataModelElement_t, if {@link name} describes a valid path. NULL otherwise.
 */
DataModelElement_t* getDescription(DataModelElement_t *root, char *name) {
	DataModelElement_t *cur = root;
	char *token = NULL, *nameCopy = NULL, *nameCopy_ = NULL;
	int found = 0, i = 0;
	
	if (root == NULL) {
		return NULL;
	}
	// Copy the search string, because strtok modifies it.
	nameCopy = (char*)ALLOC(strlen(name) + 1);
	if (!nameCopy) {
		return NULL;
	}
	nameCopy_ = nameCopy;
	strcpy(nameCopy,name);

	token = strsep(&nameCopy,".");
	while (token) {
		found = 0;
		// Look up th current token in the current nodes children array.
		for (i = 0; i < cur->childrenLen; i++) {
			if (strcmp(token,cur->children[i]->name) == 0) {
				// Found it. Step down and proceed with the next token.
				cur = cur->children[i];
				found = 1;
				break;
			}
		}
		if (found) {
			// Read the next token. If this was the last one, the loop will terminate and the function returns a pointer to the description.
			token = strsep(&nameCopy,".");
		} else {
			break;
		}
	};
	FREE(nameCopy_);
	if (found) {
		return cur;
	}	
	
	return NULL;
};
#ifdef __KERNEL__
EXPORT_SYMBOL(getDescription);
#endif

/**
 * Frees all memory used by the subtree including {@link node}.
 * @param node The root of the subtree to be freed
 */
void freeDataModel(DataModelElement_t *node, int freeNodeItself) {
	DataModelElement_t *curNode = node, *parent = NULL;
	int i = 0, j = 0;

	do {
		// First, step down until the first leaf is reached.
		if (curNode->childrenLen > 0) {
				curNode = curNode->children[0];
		} else {
			// Second, look for the current nodes sibling.
			while(curNode != node) {
				j = -1;
				parent = curNode->parent;
				for (i = 0; i < parent->childrenLen; i++) {
					if (parent->children[i] == curNode) {
						freeNode(parent->children[i],freeNodeItself);
						parent->children[i] = NULL;
						j = i;
						break;
					}
				}
				if (j == parent->childrenLen - 1) {
					// If there is none, go one level up and look for this nodes sibling.
					curNode = parent;
				} else {
					// At least one sibling left. Go for it.
					j++;
					curNode = parent->children[j];
					break;
				}
			// Stop, if the root node is reached.
			};
		}
	} while(curNode != node);
	freeNode(curNode,freeNodeItself);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(freeDataModel);
#endif

/**
 * Calculate the size in bytes of the element described by {@link typeDesc}.
 * @param typeDesc
 * @return The size of this type in bytes or -1, if the size of a subtype cannot be calculated.
 */
static int getComplexTypeSize(DataModelElement_t *rootDM, DataModelElement_t *typeDesc) {
	int size = 0, i = 0, ret = 0;

	for (i = 0; i < typeDesc->childrenLen; i++) {
		if (typeDesc->children[i]->dataModelType & ARRAY) {
			size += SIZE_ARRAY;
		} else if (typeDesc->children[i]->dataModelType & INT) {
			size += SIZE_INT;
		} else if (typeDesc->children[i]->dataModelType & BYTE) {
			size += SIZE_BYTE;
		} else if (typeDesc->children[i]->dataModelType & STRING) {
			size += SIZE_STRING;
		} else if (typeDesc->children[i]->dataModelType & FLOAT) {
			size += SIZE_FLOAT;
		} else if (typeDesc->children[i]->dataModelType & COMPLEX) {
			ret = getComplexTypeSize(rootDM,typeDesc->children[i]);
			if (ret == -1) {
				return -1;
			}
			size += ret;
		} else if (typeDesc->children[i]->dataModelType & REF) {
			ret = getDataModelSize(rootDM,typeDesc->children[i],0);
			if (ret == -1) {
				return -1;
			}
			size += ret;
		}
		//TODO: should handle type and array?
	}
	return size;
}

/**
 * A caller may prodive a node ({@link elem}) which points to a SOURCE, OBJECT, REF or EVENT.
 * Hence, it resolves {@link elem} to a datamodel node being a POD or a COMPLEX type.
 * Afterwards, it returns the size in bytes of this datatype.
 * @param rootDM a pointer to the root node of the datamodel
 * @param elem a pointer to a datamodel node
 * @param ignoreArray if set to 0 and the datamodel node is an array, it returns the size of the bare type (not the array)
 */
int getDataModelSize(DataModelElement_t *rootDM, DataModelElement_t *elem, int ignoreArray) {
	int size = -1, ret = 0, type = 0;

	if (elem == NULL) {
		return -1;
	}
	type = resolveType(rootDM,elem);

	if ((type & ARRAY) && ignoreArray == 0) {
		size = SIZE_ARRAY;
	} else if (type & INT) {
		size = SIZE_INT;
	} else if (type & BYTE) {
		size = SIZE_BYTE;
	} else if (type & STRING) {
		size = SIZE_STRING;
	} else if (type & FLOAT) {
		size = SIZE_FLOAT;
	} else if (type & COMPLEX) {
		ret = getComplexTypeSize(rootDM,elem);
		if (ret == -1) {
			return -1;
		}
		size = ret;
	}

	return size;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(getDataModelSize);
#endif

/**
 * Calculates the offset in bytes of {@link child} within the type (struct) {@link parent}.
 * @param parent a pointer to DataModelElement_t describing the complex type (a.k.a. struct)
 * @param child the name of the member
 * @return the offset in bytes of {@link child} within the type {@link parent}
 */
int getComplexTypeOffset(DataModelElement_t *rootDM, DataModelElement_t *parent, char *child) {
	int offset = 0, i = 0, ret = 0;

	for (i = 0; i < parent->childrenLen; i++) {
		if (strcmp(parent->children[i]->name,child) == 0) {
			return offset;
		} else {
			if (parent->children[i]->dataModelType & ARRAY) {
				offset += SIZE_ARRAY;
			} else if (parent->children[i]->dataModelType & INT) {
				offset += SIZE_INT;
			} else if (parent->children[i]->dataModelType & BYTE) {
				offset += SIZE_BYTE;
			} else if (parent->children[i]->dataModelType & STRING) {
				offset += SIZE_STRING;
			} else if (parent->children[i]->dataModelType & FLOAT) {
				offset += SIZE_FLOAT;
			} else if (parent->children[i]->dataModelType & COMPLEX) {
				ret = getComplexTypeSize(rootDM,parent->children[i]);
				if (ret == -1) {
					return -1;
				}
				offset += ret;
			} else if (parent->children[i]->dataModelType & REF) {
				ret = getDataModelSize(rootDM,parent->children[i],0);
				if (ret == -1) {
					return -1;
				}
				offset += ret;
			}
		}
	}
	return -1;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(getComplexTypeOffset);
#endif

/**
 * Copies a node and its payload. Children points to a newly allocated memory area.
 * It size will be set according to ChildrenLen. In addition, all elements are set to NULL.
 * @param node the node which should be copied
 * @return a pointer to the copied node or NULL, if there is not enough memory left.
 */
DataModelElement_t* copyNode(DataModelElement_t *node) {
	DataModelElement_t *ret = NULL;
	Event_t *evt = NULL;
	Object_t *obj = NULL;
	Source_t *src = NULL;
	int len = 0, i = 0;

	ret = (DataModelElement_t*)ALLOC(sizeof(DataModelElement_t));
	if (!ret) {
		return NULL;
	}
	memcpy(ret,node,sizeof(DataModelElement_t));
	if (node->childrenLen) {
		ret->children = ALLOC_CHILDREN_ARRAY(ret->childrenLen);
		if (!ret->children) {
			FREE(ret);
			return NULL;
		}
	} else {
		ret->children = NULL;
	}
	for (i = 0; i < ret->childrenLen; i++) {
		ret->children[i] = NULL;
	}
	
	switch (ret->dataModelType) {
		case SOURCE:
			src = ALLOC_TYPEINFO(Source_t);
			if (!src) {
				FREE(ret->children);
				FREE(ret);
				return NULL;
			}
			memcpy(src,node->typeInfo,sizeof(Source_t));
			ret->typeInfo = src;
			INIT_LOCK(src->lock);
			break;
			
		case EVENT:
			evt = ALLOC_TYPEINFO(Event_t);
			if (!evt) {
				FREE(ret->children);
				FREE(ret);
				return NULL;
			}
			memcpy(evt,node->typeInfo,sizeof(Event_t));
			ret->typeInfo = evt;
			break;
			
		case OBJECT:
			obj = ALLOC_TYPEINFO(Object_t);
			if (!obj) {
				FREE(ret->children);
				FREE(ret);
				return NULL;
			}
			memcpy(obj,node->typeInfo,sizeof(Object_t));
			ret->typeInfo = obj;
			break;
			
		case REF:
			len = strlen((const char*)node->typeInfo) + 1;
			ret->typeInfo = ALLOC(sizeof(char) * len);
			if (!ret->typeInfo) {
				FREE(ret->children);
				FREE(ret);
				return NULL;
			}
			strcpy((char*)ret->typeInfo,(char*)node->typeInfo);
			break;
			
		default: {
			// Nothing to do
		}
	}

	return ret;
}
/**
 * Free the node itself, its payload and its children array.
 * @param node
 */
void freeNode(DataModelElement_t *node, int freeNodeItself) {
	if (node->children != NULL) {
		FREE(node->children);
		node->children = NULL;
	}
	if (node->typeInfo != NULL) {
		FREE(node->typeInfo);
		node->typeInfo = NULL;
	}
	if (freeNodeItself == 1) {
		FREE(node);
	}
}
/**
 * Deletes all nodes from {@link treePresent} described in {@link treeDelete}.
 * If a node remains without any children, it will be deleted as well.
 * Hence, it is possible that the root node ({@link treePresent}) will be deleted, too.
 * Therefore {@link treePresent} is a pointer pointer. In case of a deleted root node the pointer will be
 * set to NULL.
 * @param treePresenet The node root of the datamodel the nodes should be deleted from
 * @param treeDelete the root node of datamodel describing which nodes should be deleted
 * @return -1, if the function cannot reallocate the memory for a children array. 0 on success.
 */
int deleteSubtree(DataModelElement_t **treePresent, DataModelElement_t *treeDelete) {
	DataModelElement_t *curPresent = *treePresent, *curDelete = treeDelete, **temp = NULL, *curDeletePrev = NULL;
	int i = 0, j= 0, children = 0, found = 0;

	do {
		if (curDelete->childrenLen > 0) {
			j = -1;
			// Check of the next next child of curDelete we need to find in the present tree
			for (i = 0; i < curDelete->childrenLen; i++) {
				if (curDelete->children[i] == curDeletePrev) {
					j = i;
					break;
				}
			}
			// All children were processed. Count the remaining children in the tree.
			if (j == curDelete->childrenLen - 1) {
				children = 0;
				for (i = 0; i < curPresent->childrenLen; i++) {
					if (curPresent->children[i] != NULL) {
						children++;
					}
				}
				// If at least one child is left, a new array must be allocated and its reference must be copied.
				if (children > 0) {
					temp = ALLOC_CHILDREN_ARRAY(children);
					if (temp == NULL) {
						return -ENOMEMORY;
					}
					j = 0;
					// Copy the childrens references to the recently allocated array.
					for (i = 0; i < curPresent->childrenLen; i++) {
						if (curPresent->children[i] != NULL) {
							temp[j] = curPresent->children[i];
							j++;
							
						}
					}
					// Free the old one.
					FREE(curPresent->children);
					curPresent->children = temp;
					curPresent->childrenLen = children;
					curPresent = curPresent->parent;
				} else {
					// Reached the root node. Free it and set the pointer to the root node to NULL.
					if (curPresent->parent == NULL) {
						freeNode(curPresent,1);
						*treePresent = NULL;
						// We're done.
						break;
					}
					// No children left. Hence, search for the position of curPresent in the children array of its parent.
					for (i = 0; i < curPresent->parent->childrenLen; i++) {
						if (curPresent->parent->children[i] == curPresent) {
							// Position found. Free the node and set the entry to NULL:
							curPresent = curPresent->parent;
							freeNode(curPresent->children[i],1);
							curPresent->children[i] = NULL;
							break;
						}
					}
				}
				// We are finished with this node and its children. Step one node upwards.
				curDeletePrev = curDelete;
				curDelete = curDelete->parent;
			} else {
				// Proceed to the next child of curDelete and try to find it corresponding node in the present tree.
				j++;
				found = 0;
				for (i = 0; i < curPresent->childrenLen; i++) {
					// Ommit already deleted children.
					if (curPresent->children[i] == NULL) {
						continue;
					}
					if (curPresent->children[i]->dataModelType == curDelete->children[j]->dataModelType &&
						strcmp((char*)&curPresent->children[i]->name,(char*)&curDelete->children[j]->name) == 0) {
						// Yeah, got it. Step down to process this child.
						curPresent = curPresent->children[i];
						curDeletePrev = curDelete;
						curDelete = curDelete->children[j];
						found = 1;
						break;
					}
				}
				// curDelete->children[j] was not found in curPresents children. Don't worry. Continue with the remaining nodes, the caller wants to delete.
				if (found == 0) {
					curDeletePrev = curDelete;
					curDelete = curDelete->parent;
					curPresent = curPresent->parent;
				}
			}
		} else {
			// curDelete has no children. Well... do the same stuff as few lines above: Find curDelete in its parents children array and delete it.
			for (i = 0; i < curPresent->parent->childrenLen; i++) {
				if (curPresent->parent->children[i] == curPresent) {
					curPresent = curPresent->parent;
					freeNode(curPresent->children[i],1);
					curPresent->children[i] = NULL;
					curDeletePrev = curDelete;
					curDelete = curDelete->parent;
					break;
				}
			}
		}
	// Stop, if curDelete gets beyond the root node of the 'delete' tree.
	} while (curDelete != treeDelete->parent);

	return 0;
}
/**
 * Copies the whole subtree described by {@link rootOrigin}. 
 * {@link rootOrigin} must not be of type MODEL.
 * @param rootOrigin the root of the subtree which should be copied
 * @return a pointer to the copied subtree
 */
DataModelElement_t* copySubtree(DataModelElement_t *rootOrigin) {
	DataModelElement_t *rootCopy = NULL, *curCopy = NULL, *curOrigin = rootOrigin;
	int i = 0, childrenLen = 0;
	
	curCopy = rootCopy = copyNode(curOrigin);
	if (!rootCopy) {
		return NULL;
	}
	// The caller has to assign a parent to the root node
	curCopy->parent = NULL;

	do {
		childrenLen = curOrigin->childrenLen;
		for (i = 0; i < childrenLen; i++) {
			// Copy each child from the origin subtree, where its corresponding array element is still NULL
			if (curCopy->children[i] == NULL) {
				curCopy->children[i] = copyNode(curOrigin->children[i]);
				if (!curCopy->children[i]) {
					freeDataModel(rootCopy,1);
					return NULL;
				}
				curCopy->children[i]->parent = curCopy;
				// Step down and copy its children as well
				curCopy = curCopy->children[i];
				curOrigin = curOrigin->children[i];
				break;
			}
		}
		// All children processed. Step up.
		if (i == childrenLen) {
			curOrigin = curOrigin->parent;
			curCopy = curCopy->parent;
		}
	} while(curOrigin != rootOrigin->parent);

	return rootCopy;
}
/**
 * Adds the subtree {@link newTree} as a child to {@link node}. In order to this, the children array
 * of {@link node} has to be reallocated.
 * @param node the parent node
 * @param newTree the root of the subtree, which should be added to {@link node}
 * @return 0 on succes. -1, if the either the array cannot be reallocated or the tree cannot be copied
 */
int addSubtree(DataModelElement_t  *node, DataModelElement_t *newTree) {
	DataModelElement_t *copyNewTree = NULL, **temp = NULL;

	node->childrenLen++;
	/*
	 * Try to avoid wasting memory. There may be some free space behind node->children.
	 * If not, REALLOC will allocate a new X of bytes and move the contents.
	 */
	if (node->children == NULL) {
		temp = ALLOC_CHILDREN_ARRAY(node->childrenLen);
	} else {
		temp = REALLOC_CHILDREN_ARRAY(node->children,node->childrenLen);
	}
	if (!temp) {
		node->childrenLen--;
		return -1;
	}
	node->children = temp;
	copyNewTree = copySubtree(newTree);
	if (!copyNewTree) {
		node->childrenLen--;
		/*
		 * It is unlikely that a realloc succeeds and the copy of a subtree (with several calls to malloc) will fail...
		 * Maybe it's the other way round... :-/
		 * However, we don't worry about the sizeof(DataModelElement_t**) bytes wasted at the end of node->children.
		 * It takes more time to shrink this memory again or to malloc a smaller memory area, copy the contents and free the old one.
		 * The letter case would be the worst.... 
		 */
		return -1;
	}
	node->children[node->childrenLen - 1] = copyNewTree;
	copyNewTree->parent = node;

	return 0;
}
/**
 * Merge the new datamodel {@link newTree} in to the current model {@link oldTree}.
 * If {@link justCheckSyntax} is not 0, the function performs a dry run. It just checks, if
 * {@link newTree} can be merged in to {@link oldTree}
 * @param justCheckSyntax a value different from 0 tells the function to just perform a check
 * @param oldTree the root of the current datamodel the new one should be merged into
 * @param newTree the root of new datamodel
 * @return 0 on success. A value below 0 indicates an error.
 */
int mergeDataModel(int justCheckSyntax, DataModelElement_t *oldTree, DataModelElement_t *newTree) {
	DataModelElement_t *curNodeOld = oldTree, *curNodeNew = newTree;
	Object_t *objOld = NULL, *objNew = NULL;
	int found = 0, i = 0, j = 0;

	do {
		// Different handling for the root node. The other nodes will continue her.
		if (curNodeNew->parent != NULL) {
			found = 0;
			for (i = 0; i < curNodeOld->childrenLen; i++) {
				//printf("old:%s, new:%s\n",curNodeOld->children[i]->name,curNodeNew->name);
				// Does the name of the current node match?
				if (strcmp(curNodeOld->children[i]->name,curNodeNew->name) == 0) {
					// Yes, it does. What about the node type?
					if (curNodeOld->children[i]->dataModelType == curNodeNew->dataModelType) {
						switch (curNodeOld->children[i]->dataModelType) {
							case COMPLEX:
							case SOURCE:
							case EVENT:
								// A merge on source, event and complex nodes are not allowed.
								return -ESAMENODE;
							
							case OBJECT:
								objOld = (Object_t*)curNodeOld->children[i]->typeInfo;
								objNew = (Object_t*)curNodeNew->typeInfo;
								if (objOld->identifierType != objNew->identifierType) {
									return -EOBECJTIDENT;
								}
								break;

							default: {
								// Nothing to do.
							}
						}
						/*
						 *  The current node is already present in the datamodel. Hence, it is necessary to check, 
						 *  if the children of curNodeNew exists, too. If not, they can safely be merged.
						 *  Otherwise a merge will be refused.
						 */
						if (curNodeNew->childrenLen > 0) {
							// Step one level down in the present datamodel.
							curNodeOld = curNodeOld->children[i];
							curNodeNew = curNodeNew->children[0];
						}
						found = 1;
						break;
					} else {
						// Oh no. Same name, different type. Refuse a merge!
						return -EDIFFERENTNODETYPE;
					}
				}
			}
			if (found == 0) {
				// curNodeNew is not a child of curNodeOld. Merge it in to the current datamodel.
				if (justCheckSyntax == 0) {
					addSubtree(curNodeOld,curNodeNew);
				}
				do {
					j = -1;
					// Find the sibling of curNodeNew.
					for (i = 0; i < curNodeNew->parent->childrenLen; i++) {
						if (curNodeNew->parent->children[i] == curNodeNew) {
							j = i;
							break;
						}
					}
					/*
					 *  curNodeNew is the last one. Go one level up and check, if the new curNodeNew has siblings,
					 *  which need to be processed.
					 */
					if (j == curNodeNew->parent->childrenLen - 1) {
						curNodeNew = curNodeNew->parent;
						curNodeOld = curNodeOld->parent;
					} else {
						// At least one sibling left. Go for it.
						j++;
						curNodeNew = curNodeNew->parent->children[j];
						break;
					}
				} while(curNodeNew != newTree);
			}
		} else {
			// curNodeNew points to the root node. Procceed with its first child.
			if (curNodeNew->childrenLen > 0) {
				curNodeNew = curNodeNew->children[0];
			}
		}
	} while(curNodeNew != newTree);
	
	return 0;
}
/**
 * Check the syntax of {@link rootToCheck}. {@link rootCurrent} is used to look up the names of complex datatypes used
 * by a source or an event. It might be NULL.
 * Upon an error the function stores the pointer of the faulty node in {@link errNode}, if it is not NULL.
 * @param rootCurrent the root of the current datamodel. Used to look for complex datatypes returned by sources or events. Can be NULL
 * @param rootToCheck the root node of the datamodel which syntax should be checked
 * @param errElem a pointer to a pointer, where the function can store a pointer to the faulty node
 * @return 0 on success. A value below 0 indicates an error. The absolute value indicates the type of error
 */
int checkDataModelSyntax(DataModelElement_t *rootCurrent,DataModelElement_t *rootToCheck, DataModelElement_t **errElem) {
	DataModelElement_t *curNode = NULL;
	Event_t *evt = NULL;
	Object_t *obj = NULL;
	Source_t *src = NULL;
	int numAllowedChildren = 0, allowedTypes = 0, i = 0, j = 0;

	curNode = rootToCheck;
	do {
		numAllowedChildren = allowedTypes = 0;
		if (errElem != NULL) {
			*errElem = curNode;
		}

		switch (curNode->dataModelType) {
			case MODEL:
				numAllowedChildren = GRE_ZERO;
				allowedTypes = NAMESPACE;
				if (curNode->typeInfo != NULL) {
					return -ETYPEINFO;
				}
				break;

			case NAMESPACE:
				numAllowedChildren = GRE_ZERO;
				allowedTypes = NAMESPACE | SOURCE | EVENT | OBJECT | COMPLEX;
				if (curNode->typeInfo != NULL) {
					return -ETYPEINFO;
				}
				break;

			case SOURCE:
				numAllowedChildren = ZERO;
				allowedTypes = 0;
				if (curNode->typeInfo == NULL) {
					return -ETYPEINFO;
				}
				src = (Source_t*)curNode->typeInfo;
				if (!(src->returnType & (REF | INT | STRING | FLOAT | COMPLEX | ARRAY | BYTE))) {
					return -ERETURNTYPE;
				}
				if (src->returnType & COMPLEX) {
					if (getDescription(rootToCheck,src->returnName) == NULL) {
						if (rootCurrent == NULL || getDescription(rootCurrent,src->returnName) == NULL) {
							return -ECOMPLEXTYPE;
						}
					}
				}
				if (src->callback == NULL) {
					return -ECALLBACK;
				}
				break;

			case EVENT:
				numAllowedChildren = ZERO;
				allowedTypes = 0;
				if (curNode->typeInfo == NULL) {
					return -ETYPEINFO;
				}
				evt = (Event_t*)curNode->typeInfo;
				if (!(evt->returnType & (REF | INT | STRING | FLOAT | COMPLEX | ARRAY | BYTE))) {
					return -ERETURNTYPE;
				}
				if (evt->returnType & COMPLEX) {
					if (getDescription(rootToCheck,evt->returnName) == NULL) {
						if (rootCurrent == NULL || getDescription(rootCurrent,evt->returnName) == NULL) {
							return -ECOMPLEXTYPE;
						}
					}
				}
				if (evt->activate == NULL || evt->deactivate == NULL) {
					return -ECALLBACK;
				}
				break;

			case OBJECT:
				numAllowedChildren = GEQ_ZERO;
				allowedTypes = SOURCE | EVENT;
				if (curNode->typeInfo == NULL) {
					return -ETYPEINFO;
				}
				obj = (Object_t*)curNode->typeInfo;
				if (!(obj->identifierType & (INT | STRING | FLOAT | BYTE))) {
					return -ERETURNTYPE;
				} 
				if (obj->activate == NULL || obj->deactivate == NULL || obj->status == NULL) {
					return -ECALLBACK;
				}
				break;

			case COMPLEX:
				numAllowedChildren = GRE_ZERO;
				allowedTypes = REF | INT | STRING | FLOAT | COMPLEX | ARRAY | BYTE;
				if (curNode->typeInfo != NULL) {
					return -ETYPEINFO;
				}
				break;

			case REF:
				numAllowedChildren = ZERO;
				allowedTypes = 0;
				if (curNode->typeInfo == NULL) {
					return -ETYPEINFO;
				}
				if (getDescription(rootToCheck,(char*)curNode->typeInfo) == NULL) {
					if (rootCurrent == NULL || getDescription(rootCurrent,(char*)curNode->typeInfo) == NULL) {
						return -ENOELEMENT;
					}
				}
				break;

			default: {
				numAllowedChildren = ZERO;
				allowedTypes = 0;
				if (curNode->typeInfo != NULL) {
					return -ETYPEINFO;
				}
			}
		}
		// Check for the correct number of children.
		switch (numAllowedChildren) {
			case ZERO:
				if (curNode->childrenLen != 0) {
					return -ECHILDRENNUM;
				}
				break;

			case GRE_ZERO:
				if (curNode->childrenLen <= 0) {
					return -ECHILDRENNUM;
				}
				break;

			case GEQ_ZERO:
				 if (curNode->childrenLen < 0) {
					return -ECHILDRENNUM;
				}
				break;
		}
		// The childrens type must match a certain number of types, which depends on the type of the current node.
		if (allowedTypes) {
			for (i = 0; i < curNode->childrenLen; i++) {
				if (!(curNode->children[i]->dataModelType & allowedTypes)) {
					return -ECHILDRENTYPE;
				}
			}
		}
		// First, step down until the first leaf is reached.
		if (curNode->childrenLen > 0) {
				curNode = curNode->children[0];
		} else {
			// Second, look for the current nodes sibling.
			do {
				j = -1;
				for (i = 0; i < curNode->parent->childrenLen; i++) {
					if (curNode->parent->children[i] == curNode) {
						j = i;
						break;
					}
				}
				if (j == curNode->parent->childrenLen - 1) {
					// If there is none, go one level up and look for this nodes sibling.
					curNode = curNode->parent;
				} else {
					// At least one sibling left. Go for it.
					j++;
					curNode = curNode->parent->children[j];
					break;
				}
			// Stop, if the root node is reached.
			} while(curNode != rootToCheck);
		}
	} while(curNode != rootToCheck);
	
	return 0;
}
/**
 * Calculates the amount of memory used by the subtree starting at {@link node}.
 * @return the number of bytes to store the whole subtree in one huge chunk of memory including additional information like typeinfo
 */
int calcDatamodelSize(DataModelElement_t *node) {
	DataModelElement_t *curNode = node;
	int i = 0, j = 0, size = 0;

	do {
		size += sizeof(DataModelElement_t) + sizeof(DataModelElement_t*) * curNode->childrenLen;
		switch (curNode->dataModelType) {
			case SOURCE:
				size += sizeof(Source_t);
				break;

			case EVENT:
				size += sizeof(Event_t);
				break;

			case OBJECT:
				size += sizeof(Object_t);
				break;

			case REF:
				size += strlen(curNode->typeInfo) + 1;
				break;
		}
		if (curNode->childrenLen > 0) {
			curNode = curNode->children[0];
		} else {
			// Stop, if the root node is reached.
			while(curNode != node) {
				j = -1;
				// Look for the current nodes sibling.
				for (i = 0; i < curNode->parent->childrenLen; i++) {
					if (curNode->parent->children[i] == curNode) {
						j = i;
						break;
					}
				}
				if (j == curNode->parent->childrenLen - 1) {
					// If there is none, go one level up and look for this nodes sibling.
					curNode = curNode->parent;
				} else {
					// At least one sibling left. Go for it.
					j++;
					curNode = curNode->parent->children[j];
					break;
				}
			};
		}
	// Stop, if the root node is reached.
	} while(curNode != node);
	
	return size;
}
/**
 * It works in the same manner as {@link copyNode}. Besides it does *not* dynamically allocates the
 * memory. It takes an additional parameter which is a pointer to a memory area. The function copies 
 * all information of {@link origin} to {@link freeMem}
 * @param freeMem a pointer 
 * @param origin the node which should be copied
 * @param copy a pointer to a pointer where the function stores the start address of the node
 */
static int copyNodeAdjacent(void *freeMem, DataModelElement_t *origin, DataModelElement_t **copy) {
	int i = 0, toCopy = 0;

	*copy = (DataModelElement_t*)freeMem;
	memcpy(*copy,origin,sizeof(DataModelElement_t));
	freeMem += sizeof(DataModelElement_t);

	(*copy)->parent = NULL;
	(*copy)->children = freeMem;
	freeMem += sizeof(DataModelElement_t*) * (*copy)->childrenLen;

	for (i = 0; i < (*copy)->childrenLen; i++) {
		(*copy)->children[i] = NULL;
	}
	if (origin->typeInfo != NULL) {
		switch (origin->dataModelType) {
			case SOURCE:
				toCopy = sizeof(Source_t);
				break;

			case EVENT:
				toCopy = sizeof(Event_t);
				break;

			case OBJECT:
				toCopy = sizeof(Object_t);
				break;

			case REF:
				toCopy = strlen(origin->typeInfo) + 1;
				break;
			}
		(*copy)->typeInfo = freeMem;
		memcpy((*copy)->typeInfo,origin->typeInfo,toCopy);
		freeMem += toCopy;
	}
	return freeMem - (void*)(*copy);
}
/**
 * Copies the whole datamodel subtree starting with {@link node} to one huge memory area pointed to by
 * {@freeMem}. The caller has to ensure that {@link freeMem} provides a sufficient amount of memory 
 * to hold the subtree.
 * @param node the root node of the subtree
 * @param freeMem a pointer to the beginning of the free space
 */
void copyAndCollectDatamodel(DataModelElement_t *node, void *freeMem) {
	DataModelElement_t *curCopy = NULL, *curOrigin = node;
	int i = 0, childrenLen = 0;

	/*
	 * The following loop copies each node iterative starting with the children of node.
	 * Hence, it's necessary to copy the root node first.
	 */
	freeMem += copyNodeAdjacent(freeMem,curOrigin,&curCopy);

	do {
		childrenLen = curOrigin->childrenLen;
		for (i = 0; i < childrenLen; i++) {
			// Copy each child from the origin subtree, where its corresponding array element is still NULL
			if (curCopy->children[i] == NULL) {
				freeMem += copyNodeAdjacent(freeMem,curOrigin->children[i],&curCopy->children[i]);
				curCopy->children[i]->parent = curCopy;
				// Step down and copy its children as well
				curCopy = curCopy->children[i];
				curOrigin = curOrigin->children[i];
				break;
			}
		}
		// All children processed. Step up.
		if (i == childrenLen) {
			curOrigin = curOrigin->parent;
			curCopy = curCopy->parent;
		}
	} while(curOrigin != node->parent);
}
/**
 * Traverses the subtree starting at {@link node} and rewrites each pointer in the subtree.
 * It calculates the offset of each object according to {@link oldBaseAddr}, calculates the new absolute address according to {@link newBaseAddr}
 * and writes it back.
 * @param node The root node of this subtree
 * @param oldBaseAddr The base address of the old memory area
 * @param newBaseAddr The base address of the new memory area
 */
void rewriteDatamodelAddress(DataModelElement_t *node, void *oldBaseAddr, void *newBaseAddr) {
	DataModelElement_t *curNode = NULL;
	int i = 0, j = 0;

	curNode = node;
	do {
		if (curNode->parent != NULL) {
			curNode->parent = REWRITE_ADDR(curNode->parent,oldBaseAddr,newBaseAddr);
		}
		if (curNode->typeInfo != NULL) {
			curNode->typeInfo = REWRITE_ADDR(curNode->typeInfo,oldBaseAddr,newBaseAddr);
		}
		if (curNode->childrenLen > 0) {
			// Rewrite the address of the pointer array as well the address of each child
			curNode->children = REWRITE_ADDR(curNode->children,oldBaseAddr,newBaseAddr);
			for (i = 0; i < curNode->childrenLen; i++) {
				curNode->children[i] = REWRITE_ADDR(curNode->children[i],oldBaseAddr,newBaseAddr);
			}
			curNode = curNode->children[0];
		} else {
			// Stop, if the root node is reached.
			while(curNode != node) {
				j = -1;
				// Look for the current nodes sibling.
				for (i = 0; i < curNode->parent->childrenLen; i++) {
					if (curNode->parent->children[i] == curNode) {
						j = i;
						break;
					}
				}
				if (j == curNode->parent->childrenLen - 1) {
					// If there is none, go one level up and look for this nodes sibling.
					curNode = curNode->parent;
				} else {
					// At least one sibling left. Go for it.
					j++;
					curNode = curNode->parent->children[j];
					break;
				}
			};
		}
	// Stop, if the root node is reached.
	} while(curNode != node);
}
/**
 * Calculates the size of the subtree starting at {@link root}, allocates txMemory and copies the
 * subtree to the memory location.
 * Afterwards it tries to write the message to the ringbuffer. If it fails, it will return -1.
 * If so and the caller provided {@link userCopy}, he or she can simply call this function again after a certain amount of time.
 * Using {@link userCopy} will speed up sendDatamodel, because the datamodel is not compressed and copied again.
 * @param root
 * @param add
 * @param userCopy A pointer location where the function might store the pointer to the compact data model that should be send.
 */
int sendDatamodel(DataModelElement_t *root, int type, DataModelElement_t **userCopy) {
	DataModelElement_t *copy = NULL;
	int ret = 0;

	if (!ENDPOINT_CONNECTED()) {
		DEBUG_MSG(3,"No endpoint connected. Aborting send.\n");
		return -EBADF;
	}
	if (txBuffer == NULL) {
		ERR_MSG("txBuffer not initialized. Abort sending datamodel.\n");
		return -EBADF;
	}
	if (userCopy == NULL || (userCopy != NULL && *userCopy == NULL)) {
		ret = calcDatamodelSize(root);
		copy = (DataModelElement_t*)slcmalloc(ret);
		if (copy == NULL) {
			ERR_MSG("Cannot allocate memory to copy the datamodel\n");
			return -ENOMEM;
		}
		copyAndCollectDatamodel(root,copy);
	} else {
		copy = *userCopy;
	}
	ret = ringBufferWrite(txBuffer,type,(char*)copy);
	if (ret == -1) {
		if (userCopy == NULL) {
			slcfree(copy);
		}
		return -EBUSY;
	}
	return 0;
}
