#include <common.h>
#include <datamodel.h>

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
int getTypeSize(DataModelElement_t *typeDesc) {
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
		} else if ((typeDesc->children[i]->dataModelType & COMPLEX) || (typeDesc->children[i]->dataModelType & TYPE)) {
			ret = getTypeSize(typeDesc->children[i]);
			if (ret == -1) {
				return -1;
			}
			size += ret;
		} else if (typeDesc->children[i]->dataModelType & REF) {
			size += SIZE_REF;
		}
		//TODO: should handle type and array?
	}
	return size;
}

/**
 * 
 */
int getDataModelSize(DataModelElement_t *rootDM, DataModelElement_t *elem, int ignoreArray) {
	int size = -1, ret = 0, type = 0;

	if (elem == NULL) {
		return -1;
	}
	if (elem->dataModelType == SOURCE) {
		type = ((Source_t*)elem->typeInfo)->returnType;
		if (type & COMPLEX) {
			elem = getDescription(rootDM,((Source_t*)elem->typeInfo)->returnName);
		}
	} else if (elem->dataModelType == EVENT) {
		type = ((Event_t*)elem->typeInfo)->returnType;
		if (type & COMPLEX) {
			elem = getDescription(rootDM,((Event_t*)elem->typeInfo)->returnName);
		}
	} else if (elem->dataModelType == OBJECT) {
		type = ((Object_t*)elem->typeInfo)->identifierType;
	} else {
		type = elem->dataModelType;
	}

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
	} else if ((type & COMPLEX) || (type & TYPE)) {
		ret = getTypeSize(elem);
		if (ret == -1) {
			return -1;
		}
		size = ret;
	} else if (type & REF) {
		size = SIZE_REF;
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
int getOffset(DataModelElement_t *parent, char *child) {
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
				ret = getTypeSize(parent->children[i]);
				if (ret == -1) {
					return -1;
				}
				offset += ret;
			} else if (parent->children[i]->dataModelType & REF) {
				//TODO
			}
			//TODO: should handle type and array?
		}
	}
	return -1;
}
#ifdef __KERNEL__
EXPORT_SYMBOL(getOffset);
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
							case TYPE:
							case SOURCE:
							case EVENT:
								// A merge on source, event and type nodes are not allowed.
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
				allowedTypes = NAMESPACE | SOURCE | EVENT | OBJECT | TYPE;
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

			case TYPE:
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

			case COMPLEX:
				numAllowedChildren = ZERO;
				allowedTypes = 0;
				if (curNode->typeInfo == NULL) {
					return -ETYPEINFO;
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
