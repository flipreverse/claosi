#include <query.h>
#include <stdio.h>

#define PRINT_PREDICATES(varName)	for (i = 0; i < varName->predicateLen; i++) { \
	printf("%s %s %s",varName->predicates[i]->left.value,predicateToString(varName->predicates[i]->type),varName->predicates[i]->right.value); \
	if (i < varName->predicateLen - 1) { \
		printf(","); \
	} \
}
#define PRINT_ELEMENTS(varName)	for (i = 0; i < varName->elementsLen; i++) { \
	printf("%s",varName->elements[i]->name); \
	if (i < varName->elementsLen - 1) { \
		printf(","); \
	} \
}

const char* predicateToString(int predicate) {
	switch (predicate) {
		case EQUAL: return "==";
		case LE: return "<";
		case LEQ: return "=<";
		case GE: return ">";
		case GEQ: return ">=";
		case IN: return "IN";
		default: return "X";
	}
}

const char *sizeUnitToString(int unit) {
	switch (unit) {
		case EVENTS: return "events";
		case TIME_MS: return "ms";
		case TIME_SEC: return "sec";
		default: return "n.a.";
	}
}

void printQuery(Operator_t *rootQuery) {
	int i = 0;
	Operator_t *cur = rootQuery;
	EventStream_t *evtStream = NULL;
	SourceStream_t *srcStream = NULL;
	ObjectStream_t *objStream = NULL;
	Filter_t *filter = NULL;
	Select_t *select = NULL;
	Sort_t *sort = NULL;
	Group_t *group = NULL;
	Join_t *join = NULL;
	Aggregate_t *aggregate = NULL;


	do {
		if (cur->child != NULL) {
			printf("x = ");
		} else {
			printf("    ");
		}
		switch (cur->type) {
			case GEN_SOURCE:
				srcStream = (SourceStream_t*)cur;
				printf("Source(element=%s,urgency=%u,period=%u ms)\n", srcStream->st_name,srcStream->st_urgent,srcStream->period);
				break;

			case GEN_OBJECT:
				objStream = (ObjectStream_t*)cur;
				printf("Object(element=%s,urgency=%u,objevtevents=0x%x)\n", objStream->st_name,objStream->st_urgent,objStream->objectEvents);
				break;

			case GEN_EVENT:
				evtStream = (EventStream_t*)cur;
				printf("Event(element=%s,urgency=%u)\n", evtStream->st_name,evtStream->st_urgent);
				break;

			case SELECT:
				select = (Select_t*)cur;
				printf("Select(");
				PRINT_ELEMENTS(select);
				printf(")(x)\n");
				break;

			case FILTER:
				filter = (Filter_t*)cur;
				printf("Filter(");
				PRINT_PREDICATES(filter);
				printf(")(x)\n");
				break;

			case SORT:
				sort = (Sort_t*)cur;
				printf("Sort(size=%u %s,",sort->size,sizeUnitToString(sort->sizeUnit));
				PRINT_ELEMENTS(sort);
				printf(")(x)\n");
				break;

			case GROUP:
				group = (Group_t*)cur;
				printf("Group(size=%u %s,",group->size,sizeUnitToString(group->sizeUnit));
				PRINT_ELEMENTS(group);
				printf(")(x)\n");
				break;

			case JOIN:
				join = (Join_t*)cur;
				printf("Join(%s,",join->element.name);
				PRINT_PREDICATES(join);
				printf(")(x)\n");
				break;

			case AVG:
			case MIN:
			case MAX:
				aggregate = (Aggregate_t*)cur;
				if (aggregate->op_type == AVG) {
					printf("Avg");
				} else if (aggregate->op_type == MIN) {
					printf("Min");
				} else if (aggregate->op_type == MAX) {
					printf("Max");
				}
				printf("(size=%u %s, advance=%u %s,",aggregate->size,sizeUnitToString(aggregate->sizeUnit),aggregate->advance,sizeUnitToString(aggregate->advanceUnit));
				PRINT_ELEMENTS(aggregate);
				printf(")(x)\n");
				break;
		}
		
		i++;
		cur = cur->child;
	} while(cur != NULL);
}
