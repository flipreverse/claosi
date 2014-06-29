#include <common.h>
#include <resultset.h>
#include <query.h>
#include <api.h>

static QueryID_t globalQueryID = 1;

/**
 * Applies a certain {@link predicate} to a {@link tupel}.
 * @param rootDM a pointer to the slc datamodel
 * @param predicate the predicate which should be applied
 * @param tupel a pointer to the tupel
 * @return 1 on success. 0 otherwise.
 */
static int applyPredicate(DataModelElement_t *rootDM, Predicate_t *predicate, Tupel_t *tupel) {
	int type = STRING, leftI = 0, rightI = 0;
	char leftC = 0, rightC = 0;
	#ifndef __KERNEL__
	double leftD = 0, rightD = 0;
	#endif
	void *valueLeft = NULL, *valueRight = NULL;
	DataModelElement_t *dm = NULL;

	// Resolve the memory location where the value is stored we want to compare
	#define DEFAULT_RETURN_VALUE 0
	if (predicate->left.type == STREAM) {
		GET_MEMBER_POINTER_ALGO_ONLY(tupel, rootDM, (char*)&predicate->left.value, dm, valueLeft);
		GET_TYPE_FROM_DM(dm,type);
	}
	if (predicate->right.type == STREAM) {
		GET_MEMBER_POINTER_ALGO_ONLY(tupel, rootDM, (char*)&predicate->right.value, dm, valueRight);
		GET_TYPE_FROM_DM(dm,type);
	}
	#undef DEFAULT_RETURN_VALUE
	// For now, comparison of arrays is forbidden
	if (type & ARRAY) {
		return 0;
	}
	switch (type) {
		case STRING:
			if (predicate->left.type == POD) {
				valueLeft = &predicate->left.value;
				valueRight = (char*)*(PTR_TYPE*)valueRight;
			} else {
				valueRight = &predicate->right.value;
				valueLeft = (char*)*(PTR_TYPE*)valueLeft;
			}
			if (predicate->type == EQUAL) {
				return strcmp((char*)valueLeft,(char*)valueRight) == 0;
			} else if (predicate->type == NEQ) {
				return strcmp((char*)valueLeft,(char*)valueRight) != 0;
			}
			break;

		case INT:
			if (predicate->left.type == POD) {
				if (STRTOINT((char*)&predicate->left.value,leftI) < 0) {
					return 0;
				}
				rightI = *(int*)valueRight;
			} else {
				leftI = *(int*)valueLeft;
				if (STRTOINT((char*)&predicate->right.value,rightI) < 0) {
					return 0;
				}
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
			if (predicate->left.type == POD) {
				leftD = atof((char*)&predicate->left.value);
				rightD = *(double*)valueRight;
			} else {
				leftD = *(double*)valueLeft;
				rightD = atof((char*)&predicate->right.value);
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
			if (predicate->left.type == POD) {
				if (STRTOCHAR((char*)&predicate->left.value,leftC) < 0) {
					return -EPARAM;
				}
				rightC = *(char*)valueRight;
			} else {
				leftC = *(char*)valueLeft;
				if (STRTOCHAR((char*)&predicate->right.value,rightC) < 0) {
					return -EPARAM;
				}
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

static int applyFilter(DataModelElement_t *rootDM, Filter_t *filterOperator, Tupel_t *tupel) {
	int i = 0;
	
	for (i = 0; i < filterOperator->predicateLen; i++) {
		if (applyPredicate(rootDM,filterOperator->predicates[i],tupel) == 0) {
			return 0;
		}
	}
	return 1;
}
/**
 * Deletes all items from {@link tupel} which are *not* listed in {@link selectOperator}.
 * @param rootDM a pointer to the slc datamodel
 * @param selectOperator the select statement which should be applied
 * @param tupel a pointer to the tupel
 * @return 0
 */
static int applySelect(DataModelElement_t *rootDM, Select_t *selectOperator, Tupel_t *tupel) {
	int i = 0, j = 0, found = 0;
	
	for (i = 0; i < tupel->itemLen; i++) {
		// Ommit nonexistent items
		if (tupel->items[i] == NULL) {
			continue;
		}
		found = 0;
		for (j = 0; j < selectOperator->elementsLen; j++) {
			if (strcmp((char*)&selectOperator->elements[j]->name,(char*)&tupel->items[i]->name) == 0) {
				found = 1;
			}
		}
		if (found == 0) {
			deleteItem(rootDM,tupel,i);
		}
	}
	
	return 1;
}
/**
 * Executes {@link query}. If one of the operators do not aplly to the {@link tupel}, the {@link tuple} will be freed.
 * Therefore the caller has to provide a pointer to the pointer of tupel. In the letter case the pointer will be set to NULL.
 * If the tupel passes all operatores, a queries onQueryCompleted function will be called.
 * @param rootDM a pointer to the slc datamodel
 * @param query the query to be executed
 * @param tupel 
 */
void executeQuery(DataModelElement_t *rootDM, Query_t *query, Tupel_t **tupel) {
	Operator_t *cur = NULL;

	if (query == NULL) {
		DEBUG_MSG(1,"%s: Query is NULL",__func__);
		return;
	}
	cur = query->root;
	while (cur != NULL) {
		switch (cur->type) {
			case GEN_SOURCE:
			case GEN_OBJECT:
			case GEN_EVENT:
				break;

			case FILTER:
				if (applyFilter(rootDM,(Filter_t*)cur,*tupel) == 0) {
					freeTupel(rootDM,*tupel);
					*tupel = NULL;
					return;
				}
				break;

			case SELECT:
				if (applySelect(rootDM,(Select_t*)cur,*tupel) == 0) {
					freeTupel(rootDM,*tupel);
					*tupel = NULL;
					return;
				}
				break;

			case SORT:
			case GROUP:
				break;

			case JOIN:
				break;

			case MAX:
			case MIN:
			case AVG:
				break;
		}
		cur = cur->child;
	}
	if (query->onQueryCompleted != NULL && *tupel != NULL) {
		query->onQueryCompleted(query->queryID,*tupel);
	}
}
/**
 * By default the function will just the memory which is definitely allocated by a *malloc, e.g.
 * a predicates pointer array. If {@link freeOperator} is not zero, the operator itself will be freed, too.
 * @param op a pointer to the first operator which should be freed
 * @param freeOperator indicates, if an operator should be freed as well
 */
void freeQuery(Operator_t *op, int freeOperator) {
	Operator_t *cur = op, *prev = NULL;

	do {
		switch (cur->type) {
			case FILTER:
				if (((Filter_t*)cur)->predicateLen > 0) {
					FREE(((Filter_t*)cur)->predicates);
				}
				break;
				
			case SELECT:
				if (((Select_t*)cur)->elementsLen > 0) {
					FREE(((Select_t*)cur)->elements);
				}
				break;
				
			case SORT:
				if (((Sort_t*)cur)->elementsLen > 0) {
					FREE(((Sort_t*)cur)->elements);
				}
				break;
				
			case GROUP:
				if (((Group_t*)cur)->elementsLen > 0) {
					FREE(((Group_t*)cur)->elements);
				}
				break;
				
			case JOIN:
				if (((Join_t*)cur)->predicateLen > 0) {
					FREE(((Join_t*)cur)->predicates);
				}
				break;
				
			case MIN:
			case MAX:
			case AVG:
				if (((Group_t*)cur)->elementsLen > 0) {
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
EXPORT_SYMBOL(freeQuery);
#endif

/*
 * TODO: maybe someone wanna do a more sophisticated syntax check. For now, this will do.
 * Further checks:
 * - a IN b: a and b must refer to objects
 * - one operand is a POD and the other one refers to an object. In this case all predicate types are allowed except IN.
 * - ...
 */
#if 0
#define CHECK_PREDICATES(varOperator,varDM,tempVarDM)	for (j = 0; j < varOperator->predicateLen; j++) { \
	switch (varOperator->predicates[j]->left.type) { \
		case POD: \
			break; \
		case STREAM: \
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
		case POD: \
			break; \
		case STREAM: \
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
 * @return 0 on success and a value below zero, if an error was discovered.
 */
int checkQuerySyntax(DataModelElement_t *rootDM, Operator_t *rootQuery, Operator_t **errOperator) {
	int i = 0, j = 0;
	Operator_t *cur = rootQuery;
	GenStream_t *stream = NULL;
	SourceStream_t *srcStream = NULL;
	ObjectStream_t *objStream = NULL;
	Filter_t *filter = NULL;
	Select_t *select = NULL;
	Sort_t *sort = NULL;
	Join_t *join = NULL;
	Aggregate_t *aggregate = NULL;
	DataModelElement_t *dm = NULL;


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
					if (srcStream->frequency == 0) {
						return -ENOFERQ;
					}
				} else if (cur->type == GEN_EVENT) {
					if (dm->dataModelType != EVENT) {
						return -EWRONGSTREAMTYPE;
					}
				} else if (cur->type == GEN_OBJECT) {
					if (dm->dataModelType != OBJECT) {
						return -EWRONGSTREAMTYPE;
					}
					objStream = (ObjectStream_t*)cur;
					if (objStream->objectEvents > (OBJECT_CREATE | OBJECT_DELETE | OBJECT_STATUS)) {
						return -ENOOBJSTATUS;
					}
				}
				break;

			case FILTER:
				if (i == 0) {
					return -EWRONGORDER;
				}
				filter = (Filter_t*)cur;
				if (filter->predicateLen == 0) {
					return -ENOPREDICATES;
				}
				CHECK_PREDICATES(filter,rootDM,dm)
				break;

			case SELECT:
				if (i == 0) {
					return -EWRONGORDER;
				}
				select = (Select_t*)cur;
				if (select->elementsLen == 0) {
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
				if (sort->elementsLen == 0) {
					return -ENOELEMENTS;
				}
				CHECK_ELEMENTS(sort,rootDM)
				break;

			case JOIN:
				if (i == 0) {
					return -EWRONGORDER;
				}
				join = (Join_t*)cur;
				if (join->predicateLen == 0) {
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
				if (aggregate->elementsLen == 0) {
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
 * @return 0 on success and a value below zero, if an error was discovered.
 */
int checkQueries(DataModelElement_t *rootDM, Query_t *queries, Operator_t **errOperator, int syncAllowed) {
	int ret = 0;
	Query_t *cur = queries;
	
	do {
		if (cur->root == NULL) {
			return -EPARAM;
		}
		if (cur->onQueryCompleted == NULL) {
			return -ERESULTFUNCPTR;
		}
		if (syncAllowed == 0 && cur->queryType == SYNC) {
			return -EQUERYTYPE;
		}
		if ((ret = checkQuerySyntax(rootDM,cur->root,errOperator)) < 0) {
			return ret;
		}
		cur = cur->next;
	} while(cur != NULL);
	
	return 0;
}
/**
 * Adds all queries in that list to the corresponding nodes in the global datamodel.
 * If it is the first query added to a node, the activate function for that node gets called.
 * @param rootDM a pointer to the slc datamodelquery
 * @param query a pointer to the first query in that list
 * @return 0 on success and a value below zero, if an error was discovered.
 */
#ifdef __KERNEL__
int addQueries(DataModelElement_t *rootDM, Query_t *queries, unsigned long *flags) {
#else
int addQueries(DataModelElement_t *rootDM, Query_t *queries) {
#endif
	DataModelElement_t *dm = NULL;
	Query_t *cur = queries, **regQueries = NULL;
	char *name = NULL;
	int i = 0, addQuery = 1, events = 0, statusQuery = 0;
	
	do {
		addQuery = 1;
		statusQuery = 0;
		switch (cur->root->type) {
			case GEN_OBJECT:
				events = ((ObjectStream_t*)cur->root)->objectEvents;
				// Need to start the generate status thread?
				statusQuery = events & OBJECT_STATUS;
				// Status only query?
				if ((events & (OBJECT_CREATE | OBJECT_DELETE)) == 0 && events & OBJECT_STATUS) {
					// Oh yes. No need to remember this query.
					addQuery = 0;
				}
			case GEN_EVENT:
			case GEN_SOURCE:
				name = ((GenStream_t*)cur->root)->name;
				break;
		}

		dm = getDescription(rootDM,name);
		if (addQuery) {
			// This function just gets called from an api method. Hence, the lock is already acquired.
			// We can safely operate on the numQueries var.
			switch (dm->dataModelType) {
				case EVENT:
					regQueries = ((Event_t*)dm->typeInfo)->queries;
					if (((Event_t*)dm->typeInfo)->numQueries == 0) {
						((Event_t*)dm->typeInfo)->activate();
					}
					((Event_t*)dm->typeInfo)->numQueries++;
					break;

				case OBJECT:
					regQueries = ((Object_t*)dm->typeInfo)->queries;
					if (((Object_t*)dm->typeInfo)->numQueries == 0) {
						((Object_t*)dm->typeInfo)->activate();
					}
					((Object_t*)dm->typeInfo)->numQueries++;
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
			cur->queryID = MAKE_QUERY_ID(globalQueryID,i);
			globalQueryID++;
		}
		if (statusQuery) {
			#ifdef __KERNEL__
			startObjStatusThread(cur,((Object_t*)dm->typeInfo)->status,flags);
			#else
			startObjStatusThread(cur,((Object_t*)dm->typeInfo)->status);
			#endif
		}

		cur = cur->next;
	} while(cur != NULL);

	return 0;
}
/**
 * Removes all queries in that list from the corresponding nodes in the global datamodel.
 * If it is the last query removed from a node, the deactivate function for that node gets called.
 * @param rootDM a pointer to the slc datamodelquery
 * @param query a pointer to the first query in that list
 * @return 0 on success and a value below zero, if an error was discovered.
 */
int delQueries(DataModelElement_t *rootDM, Query_t *queries) {
	DataModelElement_t *dm = NULL;
	Query_t *cur = queries, **regQueries = NULL;
	char *name = NULL;
	int id = 0;
	
	do {
		switch (cur->root->type) {
			case GEN_EVENT:
			case GEN_OBJECT:
			case GEN_SOURCE:
				name = ((GenStream_t*)cur->root)->name;
				break;
		}

		dm = getDescription(rootDM,name);
		switch (dm->dataModelType) {
			case EVENT:
				regQueries = ((Event_t*)dm->typeInfo)->queries;
				((Event_t*)dm->typeInfo)->numQueries--;
				if (((Event_t*)dm->typeInfo)->numQueries == 0) {
					((Event_t*)dm->typeInfo)->deactivate();
				}
				break;

			case OBJECT:
				regQueries = ((Object_t*)dm->typeInfo)->queries;
				((Object_t*)dm->typeInfo)->numQueries--;
				if (((Object_t*)dm->typeInfo)->numQueries == 0) {
					((Object_t*)dm->typeInfo)->deactivate();
				}
				break;

			case SOURCE:
				regQueries = ((Source_t*)dm->typeInfo)->queries;
				break;
		}
		id = GET_LOCAL_QUERY_ID(cur->queryID);
		regQueries[id] = NULL;

		cur = cur->next;
	} while(cur != NULL);

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
