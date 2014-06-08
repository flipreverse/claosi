#include <resultset.h>


void printValue(DataModelElement_t *rootDM, DataModelElement_t *elem, void *value) {
	int isArray = 0, i = 0, len = 1, advance = 0, type = 0;
	void *cur = NULL;

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


	isArray = type & ARRAY;
	if (isArray) {
		value = (void*) *((PTR_TYPE*)value);
		len = *((int*)value);
		printf("{");
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
		if (type & TYPE) {
			printf("{");
			for(i = 0; i < elem->childrenLen; i++) {
				printf("%s=",elem->children[i]->name);
				printValue(rootDM,elem->children[i],cur + getOffset(elem,elem->children[i]->name));
				if(i < elem->childrenLen - 1) {
					printf(",");
				}
			}
			printf("}");
		} else if (type & STRING) {
			printf("%s",(char*)*((PTR_TYPE*)cur));
		} else if (type & FLOAT) {
			printf("%e",*(double*)cur);
		} else if (type & INT) {
			printf("%d",*(int*)cur);
		} else if (type & BYTE) {
			printf("%hhu",*((char*)cur));
		}
		if (isArray && i < len - 1) {
			printf(",");
		}
	}
	if (isArray) {
		printf("}");
	}
}

static void printItem(DataModelElement_t *rootDM, Item_t *item) {
	DataModelElement_t *elem = NULL;
	
	if ((elem = getDescription(rootDM,item->name)) == NULL) {
		printf("(null)");
	} else {
		printf("%s=",item->name);
		if (item->value != NULL) {
			printValue(rootDM,elem,item->value);
		} else {
			printf("(null)");
		}
	}
}

void printTupel(DataModelElement_t *rootDM, Tupel_t *tupel) {
	int i = 0;

	printf("%llu,isCompact=%hhd,",tupel->timestamp,IS_COMPACT(tupel));
	if (IS_COMPACT(tupel)) {
		printf("size=%d,",COMPACT_SIZE(tupel));
	}
	for(i = 0; i < tupel->itemLen; i++) {
		printItem(rootDM,tupel->items[i]);
		if (i < tupel->itemLen - 1) {
			printf(",");
		}
	}
	printf("\n");
}
