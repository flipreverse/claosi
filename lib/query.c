#include <stdlib.h>
#include <string.h>
#include <query.h>

#include <stdio.h>


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
#define CHECK_PREDICATES(varOperator,varDM,tempVarDM)	for (j = 0; j < varOperator->predicateLen; j++) { \
	switch (varOperator->predicates[j]->left.type) { \
		case POD: \
			break; \
		case DATAMODEL: \
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
						printf("1: %s\n",tempVarDM->name); \
						return -ENOTCOMPARABLE; \
					} \
				} else if (tempVarDM->dataModelType == SOURCE) { \
					if (((Source_t*)tempVarDM->typeInfo)->returnType == COMPLEX) { \
						printf("2: %s\n",tempVarDM->name); \
						return -ENOTCOMPARABLE; \
					} \
				} \
			} \
			break; \
	} \
	switch (varOperator->predicates[j]->right.type) {\
		case POD: \
			break; \
		case DATAMODEL: \
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
						printf("1: %s\n",tempVarDM->name); \
						return -ENOTCOMPARABLE; \
					} \
				} else if (tempVarDM->dataModelType == SOURCE) { \
					if (((Source_t*)tempVarDM->typeInfo)->returnType == COMPLEX) { \
						printf("2: %s\n",tempVarDM->name); \
						return -ENOTCOMPARABLE; \
					} \
				} \
			} \
			break; \
	} \
}

/*
 * TODO: maybe someone wanna do a more sophisticated syntax check. For now, this will do.
 * Further checks:
 * - A SELECT statement cannot extract single members of a complex type
 * - 
 */
#define CHECK_ELEMENTS(varOperator,varDM) for (j = 0; j < varOperator->elementsLen; j++) { \
	if (getDescription(varDM,varOperator->elements[i]->name) == NULL) { \
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
