#include <stdlib.h>
#include <string.h>
#include <resultset.h>
#include <query.h>

#include <stdio.h>

static int applyPredicate(DataModelElement_t *rootDM, Predicate_t *predicate, Tupel_t *tupel) {
	int type = STRING, leftI = 0, rightI = 0;
	char leftC = 0, rightC = 0;
	double leftD = 0, rightD = 0;
	void *valueLeft = NULL, *valueRight = NULL;
	DataModelElement_t *dm = NULL;

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
				leftI = atoi((char*)&predicate->left.value);
				rightI = *(int*)valueRight;
			} else {
				leftI = *(int*)valueLeft;
				rightI = atoi((char*)&predicate->right.value);
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
			if (predicate->left.type == POD) {
				leftD = atof((char*)&predicate->left.value);
				rightD = *(int*)valueRight;
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
			break;

		case BYTE:
			if (predicate->left.type == POD) {
				leftC = atoi((char*)&predicate->left.value);
				rightC = *(char*)valueRight;
			} else {
				leftC = *(char*)valueLeft;
				rightC = atoi((char*)&predicate->right.value);
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

static int applySelect(DataModelElement_t *rootDM, Select_t *selectOperator, Tupel_t *tupel) {
	int i = 0, j = 0, found = 0;
	
	for (i = 0; i < tupel->itemLen; i++) {
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

void executeQuery(DataModelElement_t *rootDM, Query_t *query, Tupel_t **tupel) {
	Operator_t *cur = query->root;
	
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
	if (query->onQueryCompleted != NULL) {
		query->onQueryCompleted(*tupel);
	}
}

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

int checkAndSanitizeElementPath(char *elemPath, char **elemPathSani, char **objId) {
	
	char *elemPathCopy = NULL, *token = NULL, *objIdStart = NULL, *objIdEnd = NULL;
	int strLen = strlen(elemPath);
	
	elemPathCopy = (char*)ALLOC(sizeof(char) * (strLen + 1));
	if (!elemPathCopy) {
		return -ENOMEMORY;
	}
	*elemPathSani = (char*)ALLOC(sizeof(char) * (strLen + 1));
	if (!*elemPathSani) {
		return -ENOMEMORY;
	}
	
	strcpy(elemPathCopy,elemPath);
	
	token = strtok(elemPathCopy,".");
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
		token = strtok(NULL,".");
		if (token != NULL) {
			strcat(*elemPathSani,".");
		}
	}
	FREE(elemPathCopy);
	
	return 0;
}
