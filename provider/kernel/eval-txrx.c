static EventStream_t rxStreamBase, txStreamBase;
static Query_t queryRXBase, queryTXBase;

static void printResultRx(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;
	Sample_t sample;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	sample.ts1 = tupel->timestamp;
#ifdef EVALUATION
	sample.ts2 = tupel->timestamp2;
	sample.ts3 = tupel->timestamp3;
#endif
	sample.ts4 = timeUS;
	relay_write(relayfsOutput,&sample,sizeof(sample));

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTx(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;
	Sample_t sample;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	do_gettimeofday(&time);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

	sample.ts1 = tupel->timestamp;
#ifdef EVALUATION
	sample.ts2 = tupel->timestamp2;
	sample.ts3 = tupel->timestamp3;
#endif
	sample.ts4 = timeUS;
	relay_write(relayfsOutput,&sample,sizeof(sample));

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
