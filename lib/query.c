#include <common.h>
#include <resultset.h>
#include <query.h>
#include <api.h>
#include <communication.h>

extern unsigned int *globalQueryID;

/**
 * Applies a certain {@link predicate} to a {@link tupleStream} and a second tuple {@link tupleJoin}.
 * In order to execute a join it might be necessary to retrieve the left and right value of an operand from different tuples.
 * @param rootDM a pointer to the slc datamodel
 * @param predicate the predicate which should be applied
 * @param tupleStream a pointer to the tupel
 * @param 
 * @return 1 on success. 0 otherwise.
 */
static int applyPredicate(DataModelElement_t *rootDM, Predicate_t *predicate, Tupel_t *tupleStream, Tupel_t *tupleJoin) {
	int type = STRING, typeLeft = -1, typeRight = -1, leftI = 0, rightI = 0;
	char leftC = 0, rightC = 0;
	#ifndef __KERNEL__
	double leftD = 0, rightD = 0;
	#endif
	void *valueLeft = NULL, *valueRight = NULL;
	DataModelElement_t *dm = NULL;

	if (TEST_BIT(predicate->flags,PRED_SELEC)) {
		return 1;
	}
	if (predicate->left.type == OP_POD) {
		//valueLeft = &predicate->left.value;
		valueLeft = NULL;
	} else if (predicate->left.type == OP_STREAM) {
		// Resolve the memory location where the value is stored we want to compare
		valueLeft = getMemberPointer(rootDM,tupleStream,(char*)&predicate->left.value,&dm);
		if (valueLeft == NULL) {
			return 0;
		}
		typeLeft = resolveType(rootDM,dm);
	} else if (predicate->left.type == OP_JOIN) {
		// Resolve the memory location where the value is stored we want to compare
		valueLeft = getMemberPointer(rootDM,tupleJoin,(char*)&predicate->left.value,&dm);
		if (valueLeft == NULL) {
			return 0;
		}
		typeLeft = resolveType(rootDM,dm);
	} else {
		return 0;
	}

	if (predicate->right.type == OP_POD) {
		//valueRight = &predicate->right.value;
		valueRight = NULL;
	} else if (predicate->right.type == OP_STREAM) {
		// Resolve the memory location where the value is stored we want to compare
		valueRight = getMemberPointer(rootDM,tupleStream,(char*)&predicate->right.value,&dm);
		if (valueRight == NULL) {
			return 0;
		}
		typeRight = resolveType(rootDM,dm);
	} else if (predicate->right.type == OP_JOIN) {
		// Resolve the memory location where the value is stored we want to compare
		valueRight = getMemberPointer(rootDM,tupleJoin,(char*)&predicate->right.value,&dm);
		if (valueRight == NULL) {
			return 0;
		}
		typeRight = resolveType(rootDM,dm);
	} else {
		return 0;
	}

	type = (typeLeft == -1 ? typeRight : typeLeft);
	// For now, comparison of arrays is forbidden
	if ((type & ARRAY) && type > 0) {
		return 0;
	}

	switch (type) {
		case -1:
			// Cannot determine definitely the type. Just do a simple memory compare... :-(
			if (predicate->type == EQUAL) {
				return memcmp(&predicate->left.value,&predicate->right.value,MAX_NAME_LEN) == 0;
			} else if (predicate->type == NEQ) {
				return memcmp(&predicate->left.value,&predicate->right.value,MAX_NAME_LEN) != 0;
			}
			break;

		case STRING:
			if (valueLeft == NULL) {
				valueLeft = (char*)&predicate->left.value;
			} else {
				valueLeft = (void*)*(PTR_TYPE*)valueLeft;
			}
			if (valueRight == NULL) {
				valueRight = (char*)&predicate->right.value;
			} else {
				valueRight = (void*)*(PTR_TYPE*)valueRight;
			}
			if (predicate->type == EQUAL) {
				return strcmp((char*)valueLeft,(char*)valueRight) == 0;
			} else if (predicate->type == NEQ) {
				return strcmp((char*)valueLeft,(char*)valueRight) != 0;
			}
			break;

		case INT:
			if (valueLeft == NULL) {
				if (STRTOINT((char*)&predicate->left.value,leftI) < 0) {
					return 0;
				}
			} else {
				leftI = *(int*)valueLeft;
			}
			if (valueRight == NULL) {
				if (STRTOINT((char*)&predicate->right.value,rightI) < 0) {
					return 0;
				}
			} else {
				rightI = *(int*)valueRight;
			}
			switch (predicate->type) {
				case EQUAL: return leftI == rightI;
				case NEQ: return leftI != rightI;
				case LE: return leftI < rightI;
				case LEQ: return leftI <= rightI;
				case GE: return leftI > rightI;
				case GEQ: return leftI >= rightI;
			}
			break;

		case FLOAT:
			// It is not easy to compare a double inside the kernel. It will be rejected. TODO
			#ifdef __KERNEL__
			return 0;
			#else
			if (valueLeft == NULL) {
				leftD = atof((char*)&predicate->left.value);
			} else {
				leftD = *(double*)valueLeft;
			}
			if (valueRight == NULL) {
				rightD = atof((char*)&predicate->right.value);
			} else {
				rightD = *(double*)valueRight;
			}
			switch (predicate->type) {
				case EQUAL: return leftD == rightD;
				case NEQ: return leftD != rightD;
				case LE: return leftD < rightD;
				case LEQ: return leftD <= rightD;
				case GE: return leftD > rightD;
				case GEQ: return leftD >= rightD;
			}
			#endif
			break;

		case BYTE:
			if (valueLeft == NULL) {
				if (STRTOCHAR((char*)&predicate->left.value,leftC) < 0) {
					return -EPARAM;
				}
			} else {
				leftC = *(char*)valueLeft;
			}
			if (valueRight == NULL) {
				if (STRTOCHAR((char*)&predicate->right.value,rightC) < 0) {
					return -EPARAM;
				}
			} else {
				rightC = *(char*)valueRight;
			}
			switch (predicate->type) {
				case EQUAL: return leftC == rightC;
				case NEQ: return leftC != rightC;
				case LE: return leftC < rightC;
				case LEQ: return leftC <= rightC;
				case GE: return leftC > rightC;
				case GEQ: return leftC >= rightC;
			}
			break;
	}

	return 0;
}
/**
 * An object or source nested within one or more objects need selectors for the provider. A provider must know which instances of the parent objects
 * should be queried for the requested information, e.g. providing a device name for txBytes.
 * A GenStream_t already contains all needed selectors, but a join does not.
 * Hence, they need to be extracted from the predicates provided by a join.
 * @param rootDM a pointer to the root of the datamodel
 * @param joinDM a pointer to the node whose information should be joined
 * @param join a pointer to the join operator
 * @param tupleStream a pointer to the tuple which should be enlarged by the join
 */
static int buildSelectorsArray(DataModelElement_t *rootDM, DataModelElement_t *joinDM, Join_t *join, Tupel_t *tupleStream, Selector_t **selecArray, int *len, char *runOnce) {
	DataModelElement_t *curDM = NULL, *dm = NULL;
	Selector_t *array = NULL;
	void *value = NULL;
	int counter = 0, i = 0, j = 0, wildcards = 0;

	curDM = joinDM->parent;
	// *selecArry will be NULL, if we get called for the first time
	if (*selecArray == NULL) {
		// Count the number of objects on the path upwards until the root node.
		while (curDM != NULL) {
			if (curDM->dataModelType == OBJECT) {
				counter++;
			}
			curDM = curDM->parent;
		}
		if (counter == 0) {
			return -1;
		}
		array = ALLOC(sizeof(Selector_t) * counter);
		if (array == NULL) {
			return -1;
		}
		*selecArray = array;
		*len = counter;
		DEBUG_MSG(2,"Allocated %d selectors at %p\n",counter,array);
	} else {
		DEBUG_MSG(2,"Rebuilding selectors array with %d elements\n",*len);
		array = *selecArray;
	}
	// Now, collect each selector and parse its value
	for (curDM = joinDM->parent, i = *len - 1; curDM != NULL && i >= 0; curDM = curDM->parent) {
		if (curDM->dataModelType != OBJECT) {
			continue;
		}
		for (j = 0; j < join->predicateLen; j++) {
			counter = 0;
			if (join->predicates[j]->type != EQUAL) {
				continue;
			}
			if (join->predicates[j]->left.type == OP_JOIN && join->predicates[j]->right.type == OP_POD) {
				// the value is provided by the right operand of the i-th predicate
				dm = getDescription(rootDM,(char*)&join->predicates[j]->left.value);
				value = &join->predicates[j]->right.value;
			} else if (join->predicates[j]->right.type == OP_JOIN && join->predicates[j]->left.type == OP_POD) {
				// the value is provided by the left operand of the i-th predicate
				dm = getDescription(rootDM,(char*)&join->predicates[j]->right.value);
				value = &join->predicates[j]->left.value;
			} else if (join->predicates[j]->left.type == OP_STREAM && join->predicates[j]->right.type == OP_JOIN) {
				// the value is provided by the tuple
				value = getMemberPointer(rootDM,tupleStream,(char*)&join->predicates[j]->left.value,&dm);
				// Recycle the counter variable...
				counter = 1;
			} else if (join->predicates[j]->right.type == OP_STREAM && join->predicates[j]->left.type == OP_JOIN) {
				// the value is provided by the tuple
				value = getMemberPointer(rootDM,tupleStream,(char*)&join->predicates[j]->right.value,&dm);
				counter = 1;
			} else {
				continue;
			}

			/*
			 * counter == 1 indicates that the value is already part of the stream 
			 * and it is not necessary to parse the value from the i-th predicate
			 */
			if (dm != NULL && dm == curDM) {
				SET_BIT(join->predicates[j]->flags,PRED_SELEC);
				switch (((Object_t*)dm->typeInfo)->identifierType) {
					case INT:
						if (counter == 1) {
							memcpy((char*)&array[i].value,value,SIZE_INT);
						} else {
							STRTOINT((char*)value,(*(int*)&array[i].value));
							if (*(int*)&array[i].value == -1) {
								wildcards++;
							}
						}
						DEBUG_MSG(2,"copied int to index %d: %d\n",i,*(int*)array[i].value);
						break;

					case BYTE:
						if (counter == 1) {
							memcpy((char*)&array[i].value,value,SIZE_BYTE);
						} else {
							STRTOCHAR((char*)value,array[i].value[0]);
							if (array[i].value[0] == -1) {
								wildcards++;
							}
						}
						DEBUG_MSG(2,"copied byte to index %d: %c\n",i,array[i].value[0]);
						break;

					case FLOAT:
						#ifndef __KERNEL__
						if (counter == 1) {
							memcpy((char*)&array[i].value,value,SIZE_FLOAT);
						} else {
							*(float*)&array[i].value = atof((char*)&value);
							if (*(float*)&array[i].value == -1) {
								wildcards++;
							}
						}
						DEBUG_MSG(2,"copied float to index %d: %e\n",i,*(float*)array[i].value);
						#endif
						break;

					case STRING:
						if (counter == 1) {
							strncpy((char*)&array[i].value,(char*)*(PTR_TYPE*)value,MAX_NAME_LEN);
						} else {
							strncpy((char*)&array[i].value,(char*)value,MAX_NAME_LEN);
							if (*(char*)&array[i].value == '*') {
								wildcards++;
							}
						}
						DEBUG_MSG(2,"copied string to index %d@%p: %s\n",i,(char*)*(PTR_TYPE*)value,array[i].value);
						break;
				}
				i--;
				break;
			} else {
				//ERR_MSG("%s:%d: dm=%s, curDM=%s\n",__FUNCTION__,__LINE__,dm->name, curDM->name);
			}
		}
	}
	/*
	 * The user did not provide a sufficient amount of selectors. Abort processing.
	 */
	if (i >= 0) {
		FREE(array);
		*selecArray = NULL;
		*runOnce = 0;
		return -1;
	}
	/*
	 * #selectors has to be equal to the number of object on the path to the datamodel node.
	 * If the user set all selectors to their type-specific wildcards, tell the caller to call
	 * this function only once.
	 */
	*runOnce = wildcards == *len;
	return 0;
}

static void applyFilter(DataModelElement_t *rootDM, Filter_t *filterOperator, Tupel_t **headTuple) {
	Tupel_t *prevTuple = NULL, *curTuple = *headTuple, *nextTuple = NULL;
	int i = 0;

	while (curTuple != NULL) {
		for (i = 0; i < filterOperator->predicateLen; i++) {
			if (applyPredicate(rootDM,filterOperator->predicates[i],curTuple,NULL) == 0) {
				break;
			}
		}
		nextTuple = curTuple->next;
		if (i < filterOperator->predicateLen) {
			if (prevTuple == NULL) {
				*headTuple = nextTuple;
			} else {
				prevTuple->next = nextTuple;
			}
			freeTupel(rootDM,curTuple);
		} else {
			prevTuple = curTuple;
		}
		curTuple = nextTuple;
	}
}
/**
 * Deletes all items from {@link tupel} which are *not* listed in {@link selectOperator}.
 * @param rootDM a pointer to the slc datamodel
 * @param selectOperator the select statement which should be applied
 * @param tupel a pointer to the tupel
 * @return 0
 */
static void applySelect(DataModelElement_t *rootDM, Select_t *selectOperator, Tupel_t *headTuple) {
	Tupel_t *curTuple = headTuple;
	int i = 0, j = 0, found = 0;

	while (curTuple != NULL) {
		for (i = 0; i < curTuple->itemLen; i++) {
			// Ommit nonexistent items
			if (curTuple->items[i] == NULL) {
				continue;
			}
			found = 0;
			for (j = 0; j < selectOperator->elementsLen; j++) {
				if (strcmp((char*)&selectOperator->elements[j]->name,(char*)&curTuple->items[i]->name) == 0) {
					found = 1;
				}
			}
			if (found == 0) {
				deleteItem(rootDM,curTuple,i);
			}
		}
		curTuple = curTuple->next;
	}
}
/**
 * Allocates enough tx memory to store the tuple list starting at {@link tuple} and an instance of QueryContinue_t.
 * Collects and copies all tuples to the memory. 
 * @param query a pointer to the query that should be processed at the remote layer
 * @param tuple a pointer to the first tuple
 * @param steps the number of operators to skip before continuing execution 
 */
static void sendQueryContinue(Query_t *query, Tupel_t *tuple, int steps) {
	QueryContinue_t *queryCont = NULL;
	Tupel_t *curTuple= NULL, *tempTuple = NULL;
	void *freeMem = NULL;
	int temp = 0, size = sizeof(QueryContinue_t);

	if (!ENDPOINT_CONNECTED()) {
		DEBUG_MSG(3,"No endpoint connected. Aborting send.\n");
		goto out;
	}
	curTuple = tuple;
	// Calculate the amount of memory to allocate
	do {
		temp = getTupelSize(SLC_DATA_MODEL,curTuple);
		if (temp == -1) {
			ERR_MSG("Cannot determine size of tuple. Freeing all tuple and abort.\n");
			goto out;
		}
		size += temp;
		curTuple = curTuple->next;
	} while (curTuple != NULL);

	freeMem = slcmalloc(size);
	if (freeMem == NULL) {
		ERR_MSG("Cannot allocate txMemory for QueryContinue_t\n");
		goto out;
	}

	queryCont = (QueryContinue_t*)freeMem;
	strncpy((char*)&queryCont->qID.name,(char*)&((GenStream_t*)query->root)->name,MAX_NAME_LEN);
	queryCont->qID.id = query->queryID;
	queryCont->steps = steps;
	freeMem += sizeof(QueryContinue_t);

	curTuple = tuple;
	// Copy all tuple to the tx memory
	do {
		tempTuple = (Tupel_t*)freeMem;
		temp = copyAndCollectTupel(SLC_DATA_MODEL,curTuple,tempTuple,0);
		freeMem += temp;
		curTuple = curTuple->next;
		if (curTuple != NULL) {
			tempTuple->next = (Tupel_t*)freeMem;
		}
	} while(curTuple != NULL);
	do {
		temp = ringBufferWrite(txBuffer,MSG_QUERY_CONTINUE,(char*)queryCont);
		if (temp == -1) {
			MSLEEP(100);
		}
	} while (temp == -1);
	//Freeing origin tuple... They are no longer needed.
out:curTuple = tuple;
	while (curTuple != NULL) {
		tempTuple = curTuple->next;
		freeTupel(SLC_DATA_MODEL,curTuple);
		curTuple = tempTuple;
	}
}
/**
 * Uses the callback of the datamodel node which should be joined to retrieve the new tuples.
 * The datamodel node is resolved by using the element.name field of {@link  join}.
 * If the node is present and located on this layer, it calls the status() or callback() function.
 * Afterwards, it tries to match each tuple on the right hand side to the first tuple on the left hand side.
 * If any matches, they are merged. Otherwise it proceeds with the next tuple in the stream.
 * @param rootDM a pointer to the root of the datamodel
 * @param join a pointer to the join operator
 * @param headTupleStream a pointer pointer to the first tuple in the stream
 */
static int doJoin(DataModelElement_t *rootDM,Join_t *join, Tupel_t **headTupleStream) {
	DataModelElement_t *dm = NULL;
	Selector_t *selecs = NULL;
	Tupel_t *curTupleJoin = NULL, *nextTupleJoin = NULL, *lastTupleJoin = NULL, *curTupleStream = NULL, *prevTupleStream = NULL, *tempTuple = NULL, *nextTupleStream = NULL;
	int i = 0, len = 0, shouldMerge = 0, applied = 0, moreStreamTuple = 0, moreJoinTuple = 0;
	char createSelecOnce = 0; 
	#ifdef __KERNEL__
	unsigned long flags;
	#endif

	dm = getDescription(rootDM,(char*)&join->element.name);
	if (dm == NULL) {
		return 0;
	}
	// Node is at the remote layer. Stop execution.
	if (dm->layerCode != LAYER_CODE) {
		return 2;
	}

	nextTupleStream = *headTupleStream;
	// Now do the actual join...
	while (1) {
		// No further tuples on the right side && at least one more tuple in stream && next stream tuple is not equal to the current one && rebuild selector array new stream tuple 
		if (nextTupleJoin == NULL && nextTupleStream != NULL  && nextTupleStream != curTupleStream && createSelecOnce == 0) {
			do {
				// Rebuild the selectors array => read the selectors from the next tuple in stream
				if (buildSelectorsArray(rootDM,dm,join,nextTupleStream,&selecs,&len,&createSelecOnce) == -1) {
					goto out_error;
				}
				// Call the join node again to retrieve a new list of tuples (a.k.a right side) to join
				if (dm->dataModelType == OBJECT) {
					nextTupleJoin = ((Object_t*)dm->typeInfo)->status(selecs,len,nextTupleStream);
				} else if (dm->dataModelType == SOURCE) {
					ACQUIRE_WRITE_LOCK(((Source_t*)dm->typeInfo)->lock);
					nextTupleJoin = ((Source_t*)dm->typeInfo)->callback(selecs,len,nextTupleStream);
					RELEASE_WRITE_LOCK(((Source_t*)dm->typeInfo)->lock);
				} else {
					ERR_MSG("DM is neither a source nor an object!\n");
					goto out_error;
				}
				if (nextTupleJoin != NULL) {
					lastTupleJoin = nextTupleJoin;
					// Find the last tuple on the right hand side
					while (lastTupleJoin->next != NULL) {
						lastTupleJoin = lastTupleJoin->next;
					}
				} else {
					tempTuple = nextTupleStream->next;
					if (prevTupleStream != NULL) {
						prevTupleStream->next = NULL;
					} else {
						*headTupleStream = NULL;
					}
					freeTupel(SLC_DATA_MODEL,nextTupleStream);
					nextTupleStream = tempTuple;
				}
			} while (nextTupleJoin == NULL && nextTupleStream != NULL);
		}
		if (nextTupleStream == NULL) {
			break;
		}
		curTupleJoin = nextTupleJoin;
		curTupleStream = nextTupleStream;

		shouldMerge = 1;
		for (i = 0; i < join->predicateLen; i++) {
			if (applyPredicate(rootDM,join->predicates[i],curTupleStream,curTupleJoin) == 0) {
				shouldMerge = 0;
				break;
			}
		}
		moreStreamTuple = curTupleStream->next != NULL;
		moreJoinTuple = curTupleJoin->next != NULL;
		// left and right hand side match
		if (shouldMerge == 1) {
			if (moreStreamTuple == 1 && moreJoinTuple == 1) {
				if (curTupleJoin == lastTupleJoin) {
					if (createSelecOnce == 1) {
						/*
						 * Reached the last one on the right hand side. Copy the current right tuple and append it.
						 * There are more tuples on the left side. So, it might match with another tuple.
						 */
						while (lastTupleJoin->next != NULL) {
							lastTupleJoin = lastTupleJoin->next;
						}
						lastTupleJoin->next = copyTupel(rootDM,curTupleJoin);
						if (lastTupleJoin->next == NULL) {
							goto out_error;
						}
					}
				} else {
					/*
					 * We're in the middle of a sequence of right tuples. Hence, copy the current
					 * left tuple and insert it right after the current one, because it might match
					 * another tuple on the right side.
					 */
					tempTuple = copyTupel(rootDM,curTupleStream);
					if (tempTuple == NULL) {
						goto out_error;
					}
					tempTuple->next = curTupleStream->next;
					curTupleStream->next = tempTuple;
					
					if (createSelecOnce == 1) {
						/*
						 * We have further tuples in the stream. Therefore, copy the current tuple from the right side
						 * and append it.
						 */
						tempTuple = lastTupleJoin;
						while (tempTuple->next != NULL) {
							tempTuple = tempTuple->next;
						}
						tempTuple->next = copyTupel(rootDM,curTupleJoin);
						if (tempTuple->next == NULL) {
							goto out_error;
						}
					}
				}
			} else if (moreStreamTuple == 1 && moreJoinTuple == 0) {
				if (createSelecOnce == 1) {
					/*
					 * Reached the end of the sequence of right tuples, but got further tuples in stream to process.
					 * Copy the current right tuple and append it.
					 */
					lastTupleJoin = copyTupel(rootDM,curTupleJoin);
					if (lastTupleJoin == NULL) {
						goto out_error;
					}
					curTupleJoin->next = lastTupleJoin;
				}
			} else if (moreStreamTuple == 0 && moreJoinTuple == 1) {
				/*
				 * Reached the end of the tuples in stream, but got further tuples on the right side.
				 * Copy the current stream tuple and append it.
				 */
				tempTuple = copyTupel(rootDM,curTupleStream);
				if (tempTuple == NULL) {
					goto out_error;
				}
				tempTuple->next = curTupleStream->next;
				curTupleStream->next = tempTuple;
			} else if (moreStreamTuple == 0 && moreJoinTuple == 0) {
				// nothing to do
			}
			nextTupleJoin = curTupleJoin->next;
			nextTupleStream = curTupleStream->next;
			// Lets get serious. Merge them...
			if (mergeTuple(rootDM,&curTupleStream,curTupleJoin) < 0) {
				goto out_error;
			}
			if (prevTupleStream != NULL) {
				prevTupleStream->next = curTupleStream;
			} else {
				*headTupleStream = curTupleStream;
			}
			prevTupleStream = curTupleStream;
			applied++;
		} else {
			if (moreStreamTuple == 1 && moreJoinTuple == 1) {
				if (curTupleJoin == lastTupleJoin) {
					/*
					 * The current stream tuple does not match any tuple on the right side.
					 * Free it!
					 */
					nextTupleStream = curTupleStream->next;
					freeTupel(rootDM,curTupleStream);
					if (prevTupleStream != NULL) {
						prevTupleStream->next = nextTupleStream;
					} else {
						*headTupleStream = nextTupleStream;
					}
					/*
					 * Got further tuples in stream. Hence, move the current right tuple to the end.
					 */
					nextTupleJoin = lastTupleJoin->next;
					if (createSelecOnce == 1) {
						while (lastTupleJoin->next != NULL) {
							lastTupleJoin = lastTupleJoin->next;
						}
						lastTupleJoin->next = curTupleJoin;
						curTupleJoin->next = NULL;
					} else {
						freeTupel(rootDM,lastTupleJoin);
					}
				} else {
					/*
					 * Got further tuples on the right side. So, still use the current stream tuple, 
					 * but move the current right tuple to the end of the list.
					 */
					nextTupleStream = curTupleStream;
					nextTupleJoin = curTupleJoin->next;
					if (createSelecOnce == 1) {
						tempTuple = lastTupleJoin;
						while (tempTuple->next != NULL) {
							tempTuple = tempTuple->next;
						}
						tempTuple->next = curTupleJoin;
						curTupleJoin->next = NULL;
					} else {
						freeTupel(rootDM,curTupleJoin);
					}
				}
			} else if (moreStreamTuple == 1 && moreJoinTuple == 0) {
				/*
				 * Current stream tuple does not match. Free it.
				 * But keep the current right tuple, because there are further
				 * tuples in stream.
				 */
				if (createSelecOnce == 1) {
					nextTupleJoin = curTupleJoin;
				} else {
					nextTupleJoin = curTupleJoin->next;
					freeTupel(rootDM,curTupleJoin);
				}
				nextTupleStream = curTupleStream->next;
				if (prevTupleStream != NULL) {
					prevTupleStream->next = nextTupleStream;
				} else {
					*headTupleStream = nextTupleStream;
				}
				freeTupel(rootDM,curTupleStream);
			} else if (moreStreamTuple == 0 && moreJoinTuple == 1) {
				/*
				 * Current tuple of the right side does not match any. Hence, free it.
				 * But keep the current one on the left side.
				 */
				nextTupleStream = curTupleStream;
				nextTupleJoin = curTupleJoin->next;
				freeTupel(rootDM,curTupleJoin);
			} else if (moreStreamTuple == 0 && moreJoinTuple == 0) {
				/*
				 * Reached the end of both sides. Free the last tuples of each side.
				 */
				nextTupleStream = curTupleStream->next;
				nextTupleJoin = curTupleJoin->next;
				if (prevTupleStream != NULL) {
					prevTupleStream->next = NULL;
				} else {
					*headTupleStream = NULL;
				}
				freeTupel(rootDM,curTupleStream);
				freeTupel(rootDM,curTupleJoin);
			}
		}
	}
	FREE(selecs);
	return applied > 0;

out_error:
	if (selecs != NULL) {
		FREE(selecs);
	}
	curTupleStream = *headTupleStream;
	while (curTupleStream != NULL) {
		tempTuple = curTupleStream->next;
		freeTupel(rootDM,curTupleStream);
		curTupleStream = tempTuple;
	}
	*headTupleStream = NULL;
	while (curTupleJoin != NULL) {
		tempTuple = curTupleJoin->next;
		freeTupel(rootDM,curTupleJoin);
		curTupleJoin = tempTuple;
	}
	return 0;
}

/**
 * Executes {@link query}. If one of the operators do not aplly to the {@link tupel}, the {@link tuple} will be freed.
 * Therefore the caller has to provide a pointer to the pointer of tupel. In the letter case the pointer will be set to NULL.
 * If the tupel passes all operatores, a queries onQueryCompleted function will be called.
 * @param rootDM a pointer to the slc datamodel
 * @param query the query to be executed
 * @param tupel 
 */
void executeQuery(DataModelElement_t *rootDM, Query_t *query, Tupel_t *tupleStream, int steps) {
	Operator_t *cur = NULL;
	int counter = 0, i = 0, ret = 0;
	Tupel_t *headTupleStream = NULL, *tempTuple = NULL;

	if (query == NULL) {
		DEBUG_MSG(1,"%s: Query is NULL",__func__);
		return;
	}
	headTupleStream = tupleStream;
	if (steps != -1) {
		cur = query->root;
		// Skip the first 'steps' operators
		for (i = 0; i < steps && cur != NULL; i++) {
			cur = cur->child;
		}
		counter = steps;
		while (cur != NULL) {
			switch (cur->type) {
				case GEN_SOURCE:
				case GEN_OBJECT:
				case GEN_EVENT:
					break;

				case FILTER:
					applyFilter(rootDM,(Filter_t*)cur,&headTupleStream);
					if (headTupleStream == NULL) {
						return;
					}
					break;

				case SELECT:
					applySelect(rootDM,(Select_t*)cur,headTupleStream);
					break;

				case SORT:
				case GROUP:
					break;

				case JOIN:
					ret = doJoin(rootDM,(Join_t*)cur,&headTupleStream);
					if (ret == 0) {
						return;
					} else if (ret == 2) {
						sendQueryContinue(query,tupleStream,steps);
						return;
					}
					break;

				case MAX:
				case MIN:
				case AVG:
					break;
			}
			cur = cur->child;
			counter++;
		}
	} else {
		// The query was completely executed at the remote layer.
		DEBUG_MSG(2,"No further steps to process.\n");
	}
	// It is our query. Hence, query->onCompletedFunction should point to a valid memory location
	if (query->layerCode == LAYER_CODE) {
		if (query->onQueryCompleted != NULL) {
			while (headTupleStream != NULL) {
				tempTuple = headTupleStream->next;
				query->onQueryCompleted(query->queryID,headTupleStream);
				headTupleStream = tempTuple;
			}
		}
	} else {
		// The original query is located at the remote layer. Hand it over.
		sendQueryContinue(query,headTupleStream,-1);
	}
}
/**
 * By default the function will just the memory which is definitely allocated by a *malloc, e.g.
 * a predicates pointer array. If {@link freeOperator} is not zero, the operator itself will be freed, too.
 * It iterates over all children.
 * @param op a pointer to the first operator which should be freed
 * @param freeOperator indicates, if an operator should be freed as well
 */
void freeOperator(Operator_t *op, int freeOperator) {
	Operator_t *cur = op, *prev = NULL;

	do {
		switch (cur->type) {
			case GEN_EVENT:
			case GEN_OBJECT:
			case GEN_SOURCE:
				if (((GenStream_t*)cur)->selectorsLen > 0) {
					FREE(((GenStream_t*)cur)->selectors);
				}
				break;

			case FILTER:
				if (((Filter_t*)cur)->predicates != NULL) {
					FREE(((Filter_t*)cur)->predicates);
				}
				break;
				
			case SELECT:
				if (((Select_t*)cur)->elements != NULL) {
					FREE(((Select_t*)cur)->elements);
				}
				break;
				
			case SORT:
				if (((Sort_t*)cur)->elements != NULL) {
					FREE(((Sort_t*)cur)->elements);
				}
				break;
				
			case GROUP:
				if (((Group_t*)cur)->elements != NULL) {
					FREE(((Group_t*)cur)->elements);
				}
				break;
				
			case JOIN:
				if (((Join_t*)cur)->predicates != NULL) {
					FREE(((Join_t*)cur)->predicates);
				}
				break;
				
			case MIN:
			case MAX:
			case AVG:
				if (((Group_t*)cur)->elements != NULL) {
					FREE(((Group_t*)cur)->elements);
				}
				break;
		}
		prev = cur;
		cur = cur->child;
		if (freeOperator) {
			FREE(prev);
		}	
	} while (cur != NULL);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(freeOperator);
#endif
/**
 * Frees {@link query} and all its operators including their additional information.
 * It assumes that the operators are dynamically allocated as well. Hence, they are freed, too.
 * @param query a pointer to the query which shoulbd be freed
 */
void freeQuery(Query_t *query) {
	if ((query->flags & COMPACT) == COMPACT) {
		FREE(query);
		return;
	}
	freeOperator(query->root,1);
	FREE(query);
}
#ifdef __KERNEL__
EXPORT_SYMBOL(freeQuery);
#endif

/*
 * TODO: maybe someone wanna do a more sophisticated syntax check. For now, this will do.
 * Further checks:
 * - a IN b: a and b must refer to objects
 * - one operand is a OP_POD and the other one refers to an object. In this case all predicate types are allowed except IN.
 * - ...
 */
#if 0
#define CHECK_PREDICATES(varOperator,varDM,tempVarDM)	for (j = 0; j < varOperator->predicateLen; j++) { \
	switch (varOperator->predicates[j]->left.type) { \
		case OP_POD: \
			break; \
		case OP_STREAM: \
			if (getDescription(varDM,varOperator->predicates[j]->left.value) == NULL) { \
				return -ENOOPERAND; \
			} else { \
				if (varOperator->predicates[j]->type == IN) { \
					break; \
				} \
				if (tempVarDM->dataModelType == REF) { \
					tempVarDM = getDescription(varDM,(char*)tempVarDM->typeInfo); \
					if (tempVarDM->dataModelType == COMPLEX) { \
						return -ENOTCOMPARABLE; \
					} \
				} else if (tempVarDM->dataModelType == SOURCE) { \
					if (((Source_t*)tempVarDM->typeInfo)->returnType == COMPLEX) { \
						return -ENOTCOMPARABLE; \
					} \
				} \
			} \
			break; \
	} \
	switch (varOperator->predicates[j]->right.type) {\
		case OP_POD: \
			break; \
		case OP_STREAM: \
			if ((tempVarDM = getDescription(varDM,varOperator->predicates[j]->right.value)) == NULL) { \
				return -ENOOPERAND; \
			} else { \
				if (varOperator->predicates[j]->type == IN) { \
					break; \
				} \
				if (tempVarDM->dataModelType == REF) { \
					tempVarDM = getDescription(varDM,(char*)tempVarDM->typeInfo); \
					if (tempVarDM->dataModelType == COMPLEX) { \
						return -ENOTCOMPARABLE; \
					} \
				} else if (tempVarDM->dataModelType == SOURCE) { \
					if (((Source_t*)tempVarDM->typeInfo)->returnType == COMPLEX) { \
						return -ENOTCOMPARABLE; \
					} \
				} \
			} \
			break; \
	} \
}
#else
#define CHECK_PREDICATES(varOperator,varDM,tempVarDM)
#endif

/*
 * TODO: maybe someone wanna do a more sophisticated syntax check. For now, this will do.
 * Further checks:
 * - A SELECT statement cannot extract single members of a complex type
 * - 
 */
#define CHECK_ELEMENTS(varOperator,varDM) for (j = 0; j < varOperator->elementsLen; j++) { \
	if (getDescription(varDM,varOperator->elements[j]->name) == NULL) { \
		return -ENOELEMENT; \
	} \
}
/**
 * Does a rudimental syntax checking. Abort on error and {@link errOperator}, if not NULL, will point 
 * to the faulty operator.
 * There definitely more work to do, but for now this will do.
 * @param rootDM a pointer to the slc datamodel
 * @param rootQuery the root operator of this query
 * @param errOperator will point to the faulty operator, if not NULL
 * @param sync {@link queries} shall be executed synchronously
 * @return 0 on success and a value below zero, if an error was discovered.
 */
int checkQuerySyntax(DataModelElement_t *rootDM, Operator_t *rootQuery, Operator_t **errOperator, int sync) {
	int i = 0, j = 0, counter = 0;
	Operator_t *cur = rootQuery;
	GenStream_t *stream = NULL;
	SourceStream_t *srcStream = NULL;
	ObjectStream_t *objStream = NULL;
	Filter_t *filter = NULL;
	Select_t *select = NULL;
	Sort_t *sort = NULL;
	Join_t *join = NULL;
	Aggregate_t *aggregate = NULL;
	DataModelElement_t *dm = NULL, *dmIter = NULL;


	do {
		if (errOperator != NULL) {
			*errOperator = cur;
		}
		switch (cur->type) {
			case GEN_SOURCE:
			case GEN_OBJECT:
			case GEN_EVENT:
				if (i != 0) {
					return -EWRONGORDER;
				}
				stream = (GenStream_t*)cur;
				dm = getDescription(rootDM,stream->name);
				if (dm == NULL) {
					return -ENOELEMENT;
				}
				if (cur->type == GEN_SOURCE) {
					if (dm->dataModelType != SOURCE) {
						return -EWRONGSTREAMTYPE;
					}
					srcStream = (SourceStream_t*)cur;
					if (srcStream->period == 0) {
						return -ENOPERIOD;
					}
				} else if (cur->type == GEN_EVENT) {
					if (dm->dataModelType != EVENT) {
						return -EWRONGSTREAMTYPE;
					}
					if (sync == 1) {
						return -EPARAM;
					}
				} else if (cur->type == GEN_OBJECT) {
					if (dm->dataModelType != OBJECT) {
						return -EWRONGSTREAMTYPE;
					}
					objStream = (ObjectStream_t*)cur;
					if (objStream->objectEvents > (OBJECT_CREATE | OBJECT_DELETE | OBJECT_STATUS)) {
						return -ENOOBJSTATUS;
					}
					if (sync == 1 && (objStream->objectEvents & (OBJECT_CREATE | OBJECT_DELETE)) != 0) {
						return -EPARAM;
					}
				}
				counter = 0;
				dmIter = dm;
				// 
				if (dmIter->dataModelType == OBJECT) {
					dmIter = dmIter->parent;
				}
				while (dmIter != NULL) {
					if (dmIter->dataModelType == OBJECT) {
						counter++;
					}
					dmIter = dmIter->parent;
				};
				if (counter > stream->selectorsLen) {
					return -ESELECTORS;
				}
				break;

			case FILTER:
				if (i == 0) {
					return -EWRONGORDER;
				}
				filter = (Filter_t*)cur;
				if (filter->predicateLen == 0 || filter->predicates == NULL) {
					return -ENOPREDICATES;
				}
				CHECK_PREDICATES(filter,rootDM,dm)
				break;

			case SELECT:
				if (i == 0) {
					return -EWRONGORDER;
				}
				select = (Select_t*)cur;
				if (select->elementsLen == 0 || select->elements == NULL) {
					return -ENOELEMENTS;
				}
				CHECK_ELEMENTS(select,rootDM);
				break;

			case SORT:
			case GROUP:
				if (i == 0) {
					return -EWRONGORDER;
				}
				sort = (Sort_t*)cur;
				if (sort->sizeUnit >= SIZEUNIT_END) {
					return -EUNIT;
				}
				if (sort->size == 0) {
					return -ESIZE;
				}
				if (sort->elementsLen == 0 || sort->elements == NULL) {
					return -ENOELEMENTS;
				}
				CHECK_ELEMENTS(sort,rootDM)
				break;

			case JOIN:
				if (i == 0) {
					return -EWRONGORDER;
				}
				join = (Join_t*)cur;
				if (join->predicateLen == 0 || join->predicates == NULL) {
					return -ENOPREDICATES;
				}
				#if 0
				if ((ret = checkAndSanitizeElementPath(join->element.name,&elemPath,NULL)) < 0) {
					return ret;
				}
				dm = getDescription(rootDM,elemPath);
				FREE(elemPath);
				#else
				dm = getDescription(rootDM,join->element.name);
				#endif
				if (dm == NULL) {
					return -EJOINTYPE;
				}
				if (dm->dataModelType != SOURCE && dm->dataModelType != OBJECT) {
					return -EJOINTYPE;
				}
				CHECK_PREDICATES(join,rootDM,dm)
				break;

			case MAX:
			case MIN:
			case AVG:
				if (i == 0) {
					return -EWRONGORDER;
				}
				aggregate = (Aggregate_t*)cur;
				if (aggregate->sizeUnit >= SIZEUNIT_END || aggregate->advanceUnit >= SIZEUNIT_END) {
					return -EUNIT;
				}
				if (aggregate->size == 0 || aggregate->advance == 0) {
					return -ESIZE;
				}
				if (aggregate->elementsLen == 0 || aggregate->elements == NULL) {
					return -ENOELEMENTS;
				}
				CHECK_ELEMENTS(aggregate,rootDM)
				break;
				
			default:
				return -EWRONGOPERATOR;
		}
		
		i++;
		cur = cur->child;
	} while(cur != NULL);

	return 0;
}
/**
 * Checks each query of the query list provided by the user. Each list entry should have an associated operator, a onCompleted function pointer
 * and the query syntax should be correct.
 * @param rootDM a pointer to the slc datamodelquery
 * @param query a pointer to the first query in that list
 * @param errOperator will point to the faulty operator, if not NULL
 * @param sync {@link queries} shall be executed synchronously
 * @return 0 on success and a value below zero, if an error was discovered.
 */
int checkQueries(DataModelElement_t *rootDM, Query_t *queries, Operator_t **errOperator, int sync) {
	int ret = 0;
	Query_t *cur = queries;

	do {
		if (cur->root == NULL) {
			return -EPARAM;
		}
		if (cur->onQueryCompleted == NULL) {
			return -ERESULTFUNCPTR;
		}
		if ((ret = checkQuerySyntax(rootDM,cur->root,errOperator,sync)) < 0) {
			return ret;
		}
		cur = cur->next;
	} while(cur != NULL);
	
	return 0;
}
/**
 * Determines if a query should be transfered to the other layer.
 * @param rootDM a pointer to the root of the datamodel
 * @param dm a pointer to the datamodel element which corresponds to the orign of the stream ({@link query}->root).
 * @param query a pointer to the query which should be investigated
 * @return 1, if the query should be transfered. 0 otherwise.
 */
static int shouldTransferQuery(DataModelElement_t * rootDM, DataModelElement_t *dm, Query_t *query) {
	// Query was registered at the remote layer and send to this layer. Do nothing!
	if (query->layerCode != LAYER_CODE) {
		return 0;
	}
	// Stream origin is at the remote side. Should transfer...
	if (dm->layerCode != LAYER_CODE) {
		return 1;
	}
	return 1;
}
/**
 * Allocates memory to store {@link query} in one chunk of memory, copies {@link query} and sends
 * a MSG_QUERY_ADD to the remote layer. It will block until the message could be send.
 * @param query a pointer to the query whch should be added remotely
 */
static void sendAddQuery(Query_t *query) {
	Query_t *copy = NULL;
	int temp = 0;

	if (!ENDPOINT_CONNECTED()) {
		DEBUG_MSG(3,"No endpoint connected. Aborting send.\n");
		return;
	}
	temp = calcQuerySize(query);
	copy = slcmalloc(temp);
	if (copy == NULL) {
		ERR_MSG("Cannot allocate txMemory for query: 0x%lx\n",(unsigned long)query);
		return;
	}
	copyAndCollectQuery(query,copy);
	// To make things easier we transport the size of this query to the remote layer - for more information have a look lib/{kernel/libkernel.c,userspace/libuserspace-layer.c}:commThreadWork()
	copy->size = temp;
	do {
		temp = ringBufferWrite(txBuffer,MSG_QUERY_ADD,(char*)copy);
		if (temp == -1) {
			/*
			 * In fact, it is not a got design practice to do busy waiting.
			 * But the system relies on the fact that the other layer gets informed about changes.
			 * Therefore a message has to be send.
			 * In addition, the receiving thread at the remote layer will look for new message very
			 * frequently. So, it is very unlikely to fill the ringbuffer completely.
			 */
			MSLEEP(100);
		}
	} while (temp == -1);
	query->flags |= TRANSFERED;
}
/**
 * Sets up a QueryID_t and initializes it to represent the query {@link query} points to.
 * Afterwards it sends a MSG_QUERY_DEL message to the remote layer.
 * @param query a pointer to the query whch should be deleted remotely
 */
static void sendDelQuery(Query_t *query) {
	QueryID_t *queryID = NULL;
	int temp = 0;

	if (!ENDPOINT_CONNECTED()) {
		DEBUG_MSG(3,"No endpoint connected. Aborting send.\n");
		return;
	}
	queryID = slcmalloc(sizeof(QueryID_t));
	if (queryID == NULL) {
		ERR_MSG("Cannot allocate txMemory for QueryID_t\n");
		return;
	}
	strncpy(queryID->name,(char*)&((GenStream_t*)query->root)->name,MAX_NAME_LEN);
	queryID->id = query->queryID;
	do {
		temp = ringBufferWrite(txBuffer,MSG_QUERY_DEL,(char*)queryID);
		if (temp == -1) {
			MSLEEP(100);
		}
	} while (temp == -1);
}
/**
 * Adds all queries in that list to the corresponding nodes in the global datamodel.
 * If it is the first query added to a node, the activate function for that node gets called.
 * @param rootDM a pointer to the slc datamodelquery
 * @param query a pointer to the first query in that list
 * @return 0 on success and a value below zero, if an error was discovered.
 */
#ifdef __KERNEL__
int addQueries(DataModelElement_t *rootDM, Query_t *queries, unsigned long *__flags) {
#else
int addQueries(DataModelElement_t *rootDM, Query_t *queries) {
#endif
	DataModelElement_t *dm = NULL;
	Query_t *cur = queries, **regQueries = NULL;
	GenStream_t *stream = NULL;
	char *name = NULL;
	int i = 0, events = 0, statusQuery = 0, temp = 0;
	#ifdef __KERNEL__
	unsigned long flags = *__flags;
	#endif

	for (cur = queries;cur != NULL;cur = cur->next) {
		statusQuery = 0;
		stream = (GenStream_t*)cur->root;
		switch (stream->op_type) {
			case GEN_OBJECT:
				events = ((ObjectStream_t*)stream)->objectEvents;
				// Need to start the generate status thread?
				statusQuery = events & OBJECT_STATUS;
			case GEN_EVENT:
			case GEN_SOURCE:
				name = stream->name;
				break;
		}

		dm = getDescription(rootDM,name);
		// This function just gets called from an api method. Hence, the lock is already acquired.
		// We can safely operate on the numQueries var.
		switch (dm->dataModelType) {
			case EVENT:
				regQueries = ((Event_t*)dm->typeInfo)->queries;
				break;

			case OBJECT:
				regQueries = ((Object_t*)dm->typeInfo)->queries;
				break;

			case SOURCE:
				regQueries = ((Source_t*)dm->typeInfo)->queries;
				break;
		}
		for (i = 0; i < MAX_QUERIES_PER_DM; i++) {
			if (regQueries[i] == NULL) {
				break;
			}
		}
		if (i >= MAX_QUERIES_PER_DM) {
			return -EMAXQUERIES;
		}
		regQueries[i] = cur;
		// Only assign a new global id, if we are on its origin layer
		if (cur->layerCode == LAYER_CODE) {
			temp = __sync_fetch_and_add(globalQueryID,1);
			cur->queryID = temp;
		}
		cur->idx = i;

		if (shouldTransferQuery(rootDM,dm,cur) == 1) {
			DEBUG_MSG(2,"Transfering query to other layer: 0x%lx\n",(unsigned long)cur);
			sendAddQuery(cur);
		}

		if (dm->layerCode == LAYER_CODE) {
			DEBUG_MSG(2,"Stream origin (%s) is at our layer. Start it...\n",dm->name);

			if (statusQuery) {
				#ifdef __KERNEL__
				startObjStatusThread(cur,((Object_t*)dm->typeInfo)->status,__flags);
				#else
				startObjStatusThread(cur,((Object_t*)dm->typeInfo)->status);
				#endif
			} else {
				switch (dm->dataModelType) {
					case EVENT:
						//if (((Event_t*)dm->typeInfo)->numQueries == 0) {
							/*
							 * The slc-core component does *not* know, which steps are necessary to 'activate'
							 * an event. Therefore, the write lock will be released to avoid any kind of race conditions.
							 * For example, if the kernel has to register a kprobe it may sleep or call schedule, which
							 * is *not* allowed in an atomic context.
							 * Moreover it is safe to release the lock, because there are no onging modifications. One may argue that
							 * during this short period of time without a lock held someone else deletes this particular node (dm)
							 * from the datamodel and the following code will explode.
							 * Well.... for now, we don't care. :-D This would be a rare corner case. :-)
							 */
							RELEASE_WRITE_LOCK(slcLock);
							((Event_t*)dm->typeInfo)->activate(cur);
							ACQUIRE_WRITE_LOCK(slcLock);
							#ifdef __KERNEL__
							*__flags = flags;
							#endif
						//}
						((Event_t*)dm->typeInfo)->numQueries++;
						break;

					case OBJECT:
						//if (((Object_t*)dm->typeInfo)->numQueries == 0) {
							// Same applies here for an object.
							RELEASE_WRITE_LOCK(slcLock);
							((Object_t*)dm->typeInfo)->activate(cur);
							ACQUIRE_WRITE_LOCK(slcLock);
							#ifdef __KERNEL__
							*__flags = flags;
							#endif
						//}
						((Object_t*)dm->typeInfo)->numQueries++;
						break;

					case SOURCE:
						((Source_t*)dm->typeInfo)->numQueries++;
						startSourceTimer(dm,cur);
						break;
				}
			}
		} else {
			DEBUG_MSG(2,"Stream origin (%s) is at the remote layer. Doing nothing.\n",dm->name);
		}
	}

	return 0;
}
/**
 * Removes all queries in that list from the corresponding nodes in the global datamodel.
 * If it is the last query removed from a node, the deactivate function for that node gets called.
 * @param rootDM a pointer to the slc datamodelquery
 * @param query a pointer to the first query in that list
 * @return 0 on success and a value below zero, if an error was discovered.
 */
#ifdef __KERNEL__
int delQueries(DataModelElement_t *rootDM, Query_t *queries, unsigned long *__flags) {
#else
int delQueries(DataModelElement_t *rootDM, Query_t *queries) {
#endif 
	DataModelElement_t *dm = NULL;
	Query_t *cur = queries, **regQueries = NULL;
	GenStream_t *stream = NULL;
	char *name = NULL;
	#ifdef __KERNEL__
	unsigned long flags = *__flags;
	#endif

	for (cur = queries;cur != NULL;cur = cur->next) {
		// Only a query with a valid id can safely be removed
		if (cur->queryID <= 0) {
			ERR_MSG("Query does not have a valid id. Skipping its unregistration!\n");
			continue;
		}
		if (cur->idx >= MAX_QUERIES_PER_DM) {
			ERR_MSG("Query does not have a valid idx. Skipping its unregistration!\n");
			continue;
		}
		stream = (GenStream_t*)cur->root;
		switch (stream->op_type) {
			case GEN_EVENT:
			case GEN_OBJECT:
			case GEN_SOURCE:
				name = stream->name;
				break;
		}

		dm = getDescription(rootDM,name);
		switch (dm->dataModelType) {
			case EVENT:
				regQueries = ((Event_t*)dm->typeInfo)->queries;
				break;

			case OBJECT:
				regQueries = ((Object_t*)dm->typeInfo)->queries;
				break;

			case SOURCE:
				regQueries = ((Source_t*)dm->typeInfo)->queries;
				break;
		}
		if (dm->layerCode == LAYER_CODE) {
			DEBUG_MSG(2,"Stream origin (%s) is at our layer. Stopping it...\n",dm->name);
			switch (dm->dataModelType) {
				case EVENT:
					((Event_t*)dm->typeInfo)->numQueries--;
					//if (((Event_t*)dm->typeInfo)->numQueries == 0) {
						/*
						 * The slc-core component does *not* know, which steps are necessary to 'deactivate'
						 * an event. Therefore, the write lock will be released to avoid any kind of race conditions.
						 * For example, if the kernel has to unregister a kprobe it may sleep or call schedule, which
						 * is *not* allowed in an atomic context.
						 * Moreover it is safe to release the lock, because there are no onging modifications. One may argue that
						 * during this short period of time without a lock held someone else deletes this particular node (dm)
						 * from the datamodel and the following code will explode.
						 * Well.... for now, we don't care. :-D This would be a rare corner case. :-)
						 */
						RELEASE_WRITE_LOCK(slcLock);
						((Event_t*)dm->typeInfo)->deactivate(cur);
						ACQUIRE_WRITE_LOCK(slcLock);
						#ifdef __KERNEL__
						*__flags = flags;
						#endif
					//}
					break;

				case OBJECT:
					((Object_t*)dm->typeInfo)->numQueries--;
					//if (((Object_t*)dm->typeInfo)->numQueries == 0) {
						// Same applies here for an object.
						RELEASE_WRITE_LOCK(slcLock);
						((Object_t*)dm->typeInfo)->deactivate(cur);
						ACQUIRE_WRITE_LOCK(slcLock);
						#ifdef __KERNEL__
						*__flags = flags;
						#endif
					//}
					break;

				case SOURCE:
					stopSourceTimer(cur);
					((Source_t*)dm->typeInfo)->numQueries--;
					break;
			}
		} else {
			DEBUG_MSG(2,"Stream origin (%s) is at the remote layer. Doing nothing.\n",dm->name);
		}
		DEBUG_MSG(2,"Removing all pending query: 0x%lx\n",(unsigned long)regQueries[cur->idx]);
		delPendingQuery(regQueries[cur->idx]);
		regQueries[cur->idx] = NULL;
		// Query was registered on this layer and transfered to the remote layer
		if (cur->layerCode == LAYER_CODE && (cur->flags & TRANSFERED) == TRANSFERED) {
			DEBUG_MSG(2,"Query was transfered to the remote layer. Sending a DEL_QUERY: 0x%lx\n",(unsigned long)cur);
			sendDelQuery(cur);
		}
	}

	return 0;
}

int checkAndSanitizeElementPath(char *elemPath, char **elemPathSani, char **objId) {
	
	char *elemPathCopy = NULL, *elemPathCopy_ = NULL, *token = NULL, *objIdStart = NULL, *objIdEnd = NULL;
	int strLen = strlen(elemPath);
	
	elemPathCopy = (char*)ALLOC(sizeof(char) * (strLen + 1));
	if (!elemPathCopy) {
		return -ENOMEMORY;
	}
	elemPathCopy_ = elemPathCopy;
	*elemPathSani = (char*)ALLOC(sizeof(char) * (strLen + 1));
	if (!*elemPathSani) {
		return -ENOMEMORY;
	}
	
	strcpy(elemPathCopy,elemPath);
	
	token = strsep(&elemPathCopy,".");
	while (token) {
		objIdStart = strchr(token,'[');
		if (objIdStart != NULL) {
			if ((objIdEnd = strchr(token,']')) != NULL) {
				strLen = strlen(*elemPathSani);
				strncpy(*elemPathSani + strLen,token,objIdStart - token);
				(*elemPathSani)[strLen + objIdStart - token] = '\0';
				if (objId != NULL) {
					*objId = (char*)ALLOC(objIdEnd - objIdStart);
					if (!*objId) {
						FREE(elemPathCopy);
						FREE(*elemPathSani);
						return -ENOMEMORY;
					}
					strncpy(*objId,objIdStart + 1, objIdEnd - objIdStart - 1);
					(*objId)[objIdEnd - objIdStart] = '\0';
				}
			} else {
				FREE(elemPathCopy);
				FREE(*elemPathSani);
				*elemPathSani = NULL;
				return -1;
			}
		} else {
			strcat(*elemPathSani,token);
		}
		token = strsep(&elemPathCopy,".");
		if (token != NULL) {
			strcat(*elemPathSani,".");
		}
	}
	FREE(elemPathCopy_);
	
	return 0;
}
/**
 * Calculates the size in bytes needed to store {@link query} in one memory chunk.
 * @param query a pointer to the query which size should be calculated
 * @return the number of bytes 
 */
int calcQuerySize(Query_t *query) {
	Operator_t *cur = query->root;
	int size = 0;

	size = sizeof(Query_t);
	do {
		switch (cur->type) {
			case GEN_SOURCE:
				size += sizeof(SourceStream_t) + ((GenStream_t*)cur)->selectorsLen * sizeof(Selector_t);
				break;

			case GEN_OBJECT:
				size += sizeof(ObjectStream_t) + ((GenStream_t*)cur)->selectorsLen * sizeof(Selector_t);
				break;

			case GEN_EVENT:
				size += sizeof(EventStream_t) + ((GenStream_t*)cur)->selectorsLen * sizeof(Selector_t);
				break;

			case FILTER:
				size += sizeof(Filter_t) + ((Filter_t*)cur)->predicateLen * (sizeof(Predicate_t*) + sizeof(Predicate_t));
				break;

			case SELECT:
				size += sizeof(Select_t) + ((Select_t*)cur)->elementsLen * (sizeof(Element_t*) + sizeof(Element_t));
				break;

			case SORT:
				size += sizeof(Sort_t) + ((Sort_t*)cur)->elementsLen * (sizeof(Element_t*) + sizeof(Element_t));
				break;

			case GROUP:
				size += sizeof(Group_t) + ((Group_t*)cur)->elementsLen * (sizeof(Element_t*) + sizeof(Element_t));
				break;

			case JOIN:
				size += sizeof(Join_t) + ((Join_t*)cur)->predicateLen * (sizeof(Predicate_t*) + sizeof(Predicate_t));
				break;

			case MAX:
			case MIN:
			case AVG:
				size += sizeof(Aggregate_t) + ((Aggregate_t*)cur)->elementsLen * (sizeof(Element_t*) + sizeof(Element_t));
				break;

			default:
				break;
		}
		cur = cur->child;
	} while (cur != NULL);

	return size;
}
/**
 * Copies a {@link origin} and all its additional information,e.g. predicates or elements, to {@link freeMem}.
 * @param freeMem a pointer to the usable memory area
 * @param origin a pointer to the query we want to copy
 * @param copy 
 * @return the amount of bytes used to copy {@link origin}
 */
static int copyOperatorAdjacent(void *freeMem, Operator_t *origin, Operator_t **copy) {
	int i = 0;
	void *freeMem_ = freeMem;
	GenStream_t *streamOrigin = NULL, *streamCopy = NULL;
	Filter_t *filterOrigin = NULL, *filterCopy = NULL;
	Select_t *selectOrigin = NULL, *selectCopy = NULL;
	Sort_t *sortOrigin = NULL, *sortCopy = NULL;
	Group_t *groupOrigin = NULL, *groupCopy = NULL;
	Join_t *joinOrigin = NULL, *joinCopy = NULL;
	Aggregate_t *aggregateOrigin = NULL, *aggregateCopy = NULL;

	*copy = (Operator_t*)freeMem_;
	switch (origin->type) {
		case GEN_SOURCE:
			streamOrigin = (GenStream_t*)origin;
			streamCopy = (GenStream_t*)*copy;
			freeMem_ += sizeof(SourceStream_t);
			memcpy(streamCopy,streamOrigin,sizeof(SourceStream_t));
			streamCopy->selectors = freeMem_;
			freeMem_ += sizeof(Selector_t) * streamOrigin->selectorsLen;
			memcpy(streamCopy->selectors,streamOrigin->selectors,sizeof(Selector_t) * streamOrigin->selectorsLen);
			break;

		case GEN_OBJECT:
			streamOrigin = (GenStream_t*)origin;
			streamCopy = (GenStream_t*)*copy;
			freeMem_ += sizeof(ObjectStream_t);
			memcpy(streamCopy,streamOrigin,sizeof(ObjectStream_t));
			streamCopy->selectors = freeMem_;
			freeMem_ += sizeof(Selector_t) * streamOrigin->selectorsLen;
			memcpy(streamCopy->selectors,streamOrigin->selectors,sizeof(Selector_t) * streamOrigin->selectorsLen);
			break;

		case GEN_EVENT:
			streamOrigin = (GenStream_t*)origin;
			streamCopy = (GenStream_t*)*copy;
			freeMem_ += sizeof(EventStream_t);
			memcpy(streamCopy,streamOrigin,sizeof(EventStream_t));
			streamCopy->selectors = freeMem_;
			freeMem_ += sizeof(Selector_t) * streamOrigin->selectorsLen;
			memcpy(streamCopy->selectors,streamOrigin->selectors,sizeof(Selector_t) * streamOrigin->selectorsLen);
			break;

		case FILTER:
			filterOrigin = (Filter_t*)origin;
			filterCopy = ((Filter_t*)*copy);
			freeMem_ += sizeof(Filter_t);
			memcpy(filterCopy,filterOrigin,sizeof(Filter_t));
			filterCopy->predicates = freeMem_;
			freeMem_ += filterCopy->predicateLen * sizeof(Predicate_t*);
			for (i = 0; i < filterOrigin->predicateLen; i++) {
				filterCopy->predicates[i] = freeMem_;
				memcpy(filterCopy->predicates[i],filterOrigin->predicates[i],sizeof(Predicate_t));
				freeMem_ += sizeof(Predicate_t);
			}
			break;

		case SELECT:
			selectOrigin = (Select_t*)origin;
			selectCopy = ((Select_t*)*copy);
			freeMem_ += sizeof(Select_t);
			memcpy(selectCopy,selectOrigin,sizeof(Select_t));
			selectCopy->elements = freeMem_;
			freeMem_ += selectCopy->elementsLen * sizeof(Element_t*);
			for (i = 0; i < selectOrigin->elementsLen; i++) {
				selectCopy->elements[i] = freeMem_;
				memcpy(selectCopy->elements[i],selectOrigin->elements[i],sizeof(Element_t));
				freeMem_ += sizeof(Element_t);
			}
			break;

		case SORT:
			sortOrigin = (Sort_t*)origin;
			sortCopy = ((Sort_t*)*copy);
			freeMem_ += sizeof(Sort_t);
			memcpy(sortCopy,sortOrigin,sizeof(Sort_t));
			sortCopy->elements = freeMem_;
			freeMem_ += sortCopy->elementsLen * sizeof(Element_t*);
			for (i = 0; i < sortOrigin->elementsLen; i++) {
				sortCopy->elements[i] = freeMem_;
				memcpy(sortCopy->elements[i],sortOrigin->elements[i],sizeof(Element_t));
				freeMem_ += sizeof(Element_t);
			}
			break;

		case GROUP:
			groupOrigin = (Group_t*)origin;
			groupCopy = ((Group_t*)*copy);
			freeMem_ += sizeof(Group_t);
			memcpy(groupCopy,groupOrigin,sizeof(Group_t));
			groupCopy->elements = freeMem_;
			freeMem_ += groupCopy->elementsLen * sizeof(Element_t*);
			for (i = 0; i < groupOrigin->elementsLen; i++) {
				groupCopy->elements[i] = freeMem_;
				memcpy(groupCopy->elements[i],groupOrigin->elements[i],sizeof(Element_t));
				freeMem_ += sizeof(Element_t);
			}
			break;

		case JOIN:
			joinOrigin = (Join_t*)origin;
			joinCopy = ((Join_t*)*copy);
			freeMem_ += sizeof(Join_t);
			memcpy(joinCopy,joinOrigin,sizeof(Join_t));
			joinCopy->predicates = freeMem_;
			freeMem_ += joinCopy->predicateLen * sizeof(Predicate_t*);
			for (i = 0; i < joinOrigin->predicateLen; i++) {
				joinCopy->predicates[i] = freeMem_;
				memcpy(joinCopy->predicates[i],joinOrigin->predicates[i],sizeof(Predicate_t));
				freeMem_ += sizeof(Predicate_t);
			}
			break;

		case MAX:
		case MIN:
		case AVG:
			aggregateOrigin = (Aggregate_t*)origin;
			aggregateCopy = ((Aggregate_t*)*copy);
			freeMem_ += sizeof(Aggregate_t);
			memcpy(aggregateCopy,aggregateOrigin,sizeof(Aggregate_t));
			aggregateCopy->elements = freeMem_;
			freeMem_ += aggregateCopy->elementsLen * sizeof(Element_t*);
			for (i = 0; i < aggregateOrigin->elementsLen; i++) {
				aggregateCopy->elements[i] = freeMem_;
				memcpy(aggregateCopy->elements[i],aggregateOrigin->elements[i],sizeof(Element_t));
				freeMem_ += sizeof(Element_t);
			}
			break;

		default:
			break;
	}
	return freeMem_ - freeMem;
}
/**
 * Copies query {@link origin}, its operators and their additional information, e.g. predicates, to {@link freeMem}.
 * The caller has to ensure that {@link freeMem} points to a memory location which is large enough.
 * It sets the COMPACT flag. 
 * @param origin a pointer to the query which should be copied.
 * @param freeMem a pointer to free memory
 */
void copyAndCollectQuery(Query_t *origin, void *freeMem) {
	Query_t *curCopyQuery = NULL, *curOriginQuery = origin;
	Operator_t **curCopyOp = NULL, *curOriginOp = origin->root;

	curCopyQuery = (Query_t*)freeMem;
	memcpy(curCopyQuery,curOriginQuery,sizeof(Query_t));
	freeMem += sizeof(Query_t);
	curCopyQuery->next = NULL;
	curCopyQuery->root = NULL;
	curCopyQuery->flags |= COMPACT;
	curCopyOp = &curCopyQuery->root;

	do {
		freeMem += copyOperatorAdjacent(freeMem,curOriginOp,curCopyOp);
		curOriginOp = curOriginOp->child;
		curCopyOp = &(*curCopyOp)->child;
	} while(curOriginOp != NULL);
}
/**
 * Rewrites all pointers within {@link query}.
 * @param query a pointer to the query which should be relocated
 * @param oldBaseAddr the base address of the old memory location
 * @param newBaseAddr the base address of the new memory location
 */
void rewriteQueryAddress(Query_t *query, void *oldBaseAddr, void *newBaseAddr) {
	int i = 0;
	Operator_t *curOp = NULL;
	Filter_t *filter = NULL;
	Select_t *select = NULL;
	Sort_t *sort = NULL;
	Group_t *group = NULL;
	Join_t *join = NULL;
	Aggregate_t *aggregate = NULL;
	GenStream_t *stream = NULL;

	if (query->next != NULL) {
		query->next = REWRITE_ADDR(query->next,oldBaseAddr,newBaseAddr);
	}
	query->root = REWRITE_ADDR(query->root,oldBaseAddr,newBaseAddr);
	curOp = query->root;

	do {
		if (curOp->child != NULL) {
			curOp->child = REWRITE_ADDR(curOp->child,oldBaseAddr,newBaseAddr);
		}

		switch (curOp->type) {
			case GEN_SOURCE:
			case GEN_OBJECT:
			case GEN_EVENT:
				stream = (GenStream_t*)curOp;
				stream->selectors= REWRITE_ADDR(stream->selectors,oldBaseAddr,newBaseAddr);
				break;

			case FILTER:
				filter = (Filter_t*)curOp;
				filter->predicates = REWRITE_ADDR(filter->predicates,oldBaseAddr,newBaseAddr);
				for (i = 0; i < filter->predicateLen; i++) {
					filter->predicates[i] = REWRITE_ADDR(filter->predicates[i],oldBaseAddr,newBaseAddr);
				}
				break;

			case SELECT:
				select = (Select_t*)curOp;
				select->elements = REWRITE_ADDR(select->elements,oldBaseAddr,newBaseAddr);
				for (i = 0; i < select->elementsLen; i++) {
					select->elements[i] = REWRITE_ADDR(select->elements[i],oldBaseAddr,newBaseAddr);
				}
				break;

			case SORT:
				sort = (Sort_t*)curOp;
				sort->elements = REWRITE_ADDR(sort->elements,oldBaseAddr,newBaseAddr);
				for (i = 0; i < sort->elementsLen; i++) {
					sort->elements[i] = REWRITE_ADDR(sort->elements[i],oldBaseAddr,newBaseAddr);
				}
				break;

			case GROUP:
				group = (Group_t*)curOp;
				group->elements = REWRITE_ADDR(group->elements,oldBaseAddr,newBaseAddr);
				for (i = 0; i < group->elementsLen; i++) {
					group->elements[i] = REWRITE_ADDR(group->elements[i],oldBaseAddr,newBaseAddr);
				}
				break;

			case JOIN:
				join = (Join_t*)curOp;
				join->predicates = REWRITE_ADDR(join->predicates,oldBaseAddr,newBaseAddr);
				for (i = 0; i < join->predicateLen; i++) {
					join->predicates[i] = REWRITE_ADDR(join->predicates[i],oldBaseAddr,newBaseAddr);
				}
				break;

			case MAX:
			case MIN:
			case AVG:
				aggregate = (Aggregate_t*)curOp;
				aggregate->elements = REWRITE_ADDR(aggregate->elements,oldBaseAddr,newBaseAddr);
				for (i = 0; i < aggregate->elementsLen; i++) {
					aggregate->elements[i] = REWRITE_ADDR(aggregate->elements[i],oldBaseAddr,newBaseAddr);
				}
				break;

			default:
				break;
		}
		curOp = curOp->child;
	} while(curOp != NULL);
}
/**
 * Resolves the meta description of a query (a.k.a QueryID_t) to a pointer to a Query_t.
 * @param rootDm a pointer to the datamodel which should be used to resolve id->name
 * @param id a pointer to QueryID_t
 * @return a pointer to the real query on success, or NULL on failure.
 */
Query_t* resolveQuery(DataModelElement_t *rootDM, QueryID_t *id) {
	DataModelElement_t *dm = NULL;
	Query_t **regQueries = NULL;
	int i = 0;

	dm = getDescription(rootDM,id->name);
	if (dm == NULL) {
		return NULL;
	}
	switch (dm->dataModelType) {
		case EVENT:
			regQueries = ((Event_t*)dm->typeInfo)->queries;
			break;

		case OBJECT:
			regQueries = ((Object_t*)dm->typeInfo)->queries;
			break;

		case SOURCE:
			regQueries = ((Source_t*)dm->typeInfo)->queries;
			break;

		// wrong datamodel type
		default:
			return NULL;
	}
	for (i = 0; i < MAX_QUERIES_PER_DM; i++) {
		if (regQueries[i] != NULL) {
			// id and node name match. Got it! \o/
			if (regQueries[i]->queryID == id->id && strcmp(id->name,((GenStream_t*)regQueries[i]->root)->name) == 0) {
				return regQueries[i];
			}
		}
	}

	return NULL;
}
