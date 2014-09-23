static EventStream_t rxStreamBase, txStreamBase;
static Query_t queryRXBase, queryTXBase;

static void printResultRx(unsigned int id, Tupel_t *tupel) {

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTx(unsigned int id, Tupel_t *tupel) {

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueriesTXRX(Query_t **query) {
	Query_t *temp;

	if (*query == NULL) {
		*query = &queryRXBase;
	} else {
		temp = *query;
		while (temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = &queryRXBase;
	}

	initQuery(&queryRXBase);
	queryRXBase.onQueryCompleted = printResultRx;
	queryRXBase.root = GET_BASE(rxStreamBase);
	queryRXBase.next = &queryTXBase;
	INIT_EVT_STREAM(rxStreamBase,"net.device.onRx",1,0,NULL)
	SET_SELECTOR_STRING(rxStreamBase,0,"eth1")

	initQuery(&queryTXBase);
	queryTXBase.onQueryCompleted = printResultTx;
	queryTXBase.root = GET_BASE(txStreamBase);
	INIT_EVT_STREAM(txStreamBase,"net.device.onTx",1,0,NULL)
	SET_SELECTOR_STRING(txStreamBase,0,"eth1")
}

static void destroyQueriesTXRX(void) {
	freeOperator(GET_BASE(rxStreamBase),0);
	freeOperator(GET_BASE(txStreamBase),0);
}
