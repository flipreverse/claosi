#include <datamodel.h>
#include <stdio.h>


const char* typeToString(unsigned short pType) {
	unsigned short type = 0;
	
	if (pType & ARRAY) {
		type = pType & ~ARRAY;
		switch (type) {
			case STRING:
				return "str[]";
			case INT:
				return "int[]";
			case FLOAT:
				return "float[]";
			case BYTE:
				return "byte[]";
			default:
				return "n.a.[]";
		}
	} else {
		switch (pType) {
			case STRING:
				return "str";
			case INT:
				return "int";
			case FLOAT:
				return "float";
			case BYTE:
				return "byte";
			default:
				return "n.a.";
		}
	}
};

/**
 * Prints the datamodel {@link root} to stdout.
 * @param root the root node of the datamodel
 */
void printDatamodel(DataModelElement_t *root) {
	Event_t *evt = NULL;
	Object_t *obj = NULL;
	Source_t *src = NULL;
	DataModelElement_t *curNode = NULL;
	int numTabs = 0, i = 0, j = 0;

	curNode = root;
	do {
		for (i = 0; i < numTabs; i++) {
			printf("\t");
		}
		switch (curNode->dataModelType) {
			case MODEL:
				printf("model {\n");
				break;

			case NAMESPACE:
				printf("namespace %s {\n",curNode->name);
				break;

			case SOURCE:
				src = (Source_t*)curNode->typeInfo;
				printf("source %s : ",curNode->name);
				if (src->returnType & COMPLEX) {
					printf("%s",src->returnName);
					if (src->returnType & ARRAY) {
						printf("[]");
					}
				} else {
					printf("%s",typeToString(src->returnType));
				}
				printf(" @ getSource = %p;\n", src->callback);
				break;

			case EVENT:
				evt = (Event_t*)curNode->typeInfo;
				printf("event %s : ",curNode->name);
				if (evt->returnType & COMPLEX) {
					printf("%s",evt->returnName);
					if (evt->returnType & ARRAY) {
						printf("[]");
					}
				} else {
					printf("%s",typeToString(evt->returnType));
				}
				printf(" @ activate = %p, deactivate = %p;\n", evt->activate, evt->deactivate);
				break;

			case OBJECT:
				obj = (Object_t*)curNode->typeInfo;
				printf("object %s[%s] @ activate = %p, deactivate = %p, status = %p {\n", curNode->name, typeToString(obj->identifierType), obj->activate, obj->deactivate, obj->status);
				break;

			case COMPLEX:
				printf("complex %s {\n",curNode->name);
				break;

			case REF:
				printf("reference %s (-> %s);\n",curNode->name,(char*)curNode->typeInfo);
				break;

			default: {
				printf("%s %s;\n", typeToString(curNode->dataModelType), curNode->name);
			}
		}

		// First, step down until the first leaf is reached.
		if (curNode->childrenLen > 0) {
				curNode = curNode->children[0];
				numTabs++;
		} else {
			// Second, look for the current nodes sibling.
			 while(curNode != root) {
				j = -1;
				for (i = 0; i < curNode->parent->childrenLen; i++) {
					if (curNode->parent->children[i] == curNode) {
						j = i;
						break;
					}
				}
				if (j == curNode->parent->childrenLen - 1) {
					if (curNode->parent->dataModelType == MODEL || curNode->parent->dataModelType == NAMESPACE || curNode->parent->dataModelType == OBJECT || curNode->parent->dataModelType == COMPLEX) {
						for (i = 0; i < numTabs - 1; i++) {
							printf("\t");
						}
						printf("}\n");
					}
					// If there is none, go one level up and look for this nodes sibling.
					curNode = curNode->parent;
					numTabs--;
				} else {
					// At least one sibling left. Go for it.
					j++;
					curNode = curNode->parent->children[j];
					break;
				}
			// Stop, if the root node is reached.
			};
		}
	} while(curNode != root);
}
