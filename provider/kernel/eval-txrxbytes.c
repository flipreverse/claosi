static SourceStream_t rxBytesSrc, txBytesSrc;
static Query_t queryRXBytes, queryTXBytes;

static void printResultRxBytes(unsigned int id, Tupel_t *tupel) {

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTxBytes(unsigned int id, Tupel_t *tupel) {

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueriesTXRXBytes(Query_t **query) {
	Query_t *temp;

	if (*query == NULL) {
		*query = &queryRXBytes;
	} else {
		temp = *query;
		while (temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = &queryRXBytes;
	}

	initQuery(&queryRXBytes);
	queryRXBytes.onQueryCompleted = printResultRxBytes;
	queryRXBytes.root = GET_BASE(rxBytesSrc);
	queryRXBytes.next = &queryTXBytes;
	INIT_SRC_STREAM(rxBytesSrc,"net.device.rxBytes",1,0,NULL,1000)
	SET_SELECTOR_STRING(rxBytesSrc,0,devName)

	initQuery(&queryTXBytes);
	queryTXBytes.onQueryCompleted = printResultTxBytes;
	queryTXBytes.root = GET_BASE(txBytesSrc);
	INIT_SRC_STREAM(txBytesSrc,"net.device.txBytes",1,0,NULL,1000)
	SET_SELECTOR_STRING(txBytesSrc,0,devName)
}

static void destroyQueriesTXRXBytes(void) {
	freeOperator(GET_BASE(rxBytesSrc),0);
	freeOperator(GET_BASE(txBytesSrc),0);
}
