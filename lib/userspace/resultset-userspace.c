#include <resultset.h>


void printValue(DataModelElement_t *rootDM, DataModelElement_t *elem, void *value) {
	int isArray = 0, i = 0, len = 1, advance = 0, type = 0, run = 0;
	void *cur = NULL;

	do {
		run = 0;
		if (elem->dataModelType == REF) {
			elem = getDescription(rootDM,(char*)elem->typeInfo);
			run = 1;
		} else if (elem->dataModelType == SOURCE) {
			type = ((Source_t*)elem->typeInfo)->returnType;
			if (type & COMPLEX) {
				elem = getDescription(rootDM,((Source_t*)elem->typeInfo)->returnName);
				run = 1;
			}
		} else if (elem->dataModelType == EVENT) {
			type = ((Event_t*)elem->typeInfo)->returnType;
			if (type & COMPLEX) {
				elem = getDescription(rootDM,((Event_t*)elem->typeInfo)->returnName);
				run = 1;
			}
		} else if (elem->dataModelType == OBJECT) {
			type = ((Object_t*)elem->typeInfo)->identifierType;
		} else {
			type = elem->dataModelType;
		}
	} while (run == 1);

	isArray = type & ARRAY;
	if (isArray) {
		value = (void*) *((PTR_TYPE*)value);
		len = *((int*)value);
		PRINT_MSG("{");
		value += SIZE_INT;
		if (type & STRING) {
			advance = SIZE_STRING;
		} else if (type & FLOAT) {
			advance = SIZE_FLOAT;
		} else if (type & INT) {
			advance = SIZE_INT;
		} else if (type & BYTE) {
			advance = SIZE_BYTE;
		}
	}
	for(i = 0; i < len; i++) {
		cur = value + i * advance;
		if (type & COMPLEX) {
			PRINT_MSG("{");
			for(i = 0; i < elem->childrenLen; i++) {
				PRINT_MSG("%s=",elem->children[i]->name);
				printValue(rootDM,elem->children[i],cur + getComplexTypeOffset(rootDM,elem,elem->children[i]->name));
				if(i < elem->childrenLen - 1) {
					PRINT_MSG(",");
				}
			}
			PRINT_MSG("}");
		} else if (type & STRING) {
			PRINT_MSG("%s",(char*)*((PTR_TYPE*)cur));
		} else if (type & FLOAT) {
			PRINT_MSG("%e",*(double*)cur);
		} else if (type & INT) {
			PRINT_MSG("%d",*(int*)cur);
		} else if (type & BYTE) {
			PRINT_MSG("%hhu",*((char*)cur));
		}
		if (isArray && i < len - 1) {
			PRINT_MSG(",");
		}
	}
	if (isArray) {
		PRINT_MSG("}");
	}
}

static void printItem(DataModelElement_t *rootDM, Item_t *item) {
	DataModelElement_t *elem = NULL;
	
	if ((elem = getDescription(rootDM,item->name)) == NULL) {
		PRINT_MSG("(null)");
	} else {
		PRINT_MSG("%s=",item->name);
		if (item->value != NULL) {
			printValue(rootDM,elem,item->value);
		} else {
			PRINT_MSG("(null)");
		}
	}
}

void printTupel(DataModelElement_t *rootDM, Tupel_t *tupel) {
	int i = 0;

	PRINT_MSG("%llu,isCompact=%hhd,",tupel->timestamp,TEST_BIT(tupel->flags,TUPLE_COMPACT));

	for(i = 0; i < tupel->itemLen; i++) {
		if (tupel->items[i] == NULL) {
			continue;
		}
		printItem(rootDM,tupel->items[i]);
		if (i < tupel->itemLen - 1) {
			PRINT_MSG(",");
		}
	}
	PRINT_MSG("\n");
}
