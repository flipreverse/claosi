static SourceStream_t rxBytesSrc, txBytesSrc;
static Query_t queryRXBytes, queryTXBytes;

static void printResultRxBytes(unsigned int id, Tupel_t *tupel) {
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

static void printResultTxBytes(unsigned int id, Tupel_t *tupel) {
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
	SET_SELECTOR_STRING(rxBytesSrc,0,"eth1")

	initQuery(&queryTXBytes);
	queryTXBytes.onQueryCompleted = printResultTxBytes;
	queryTXBytes.root = GET_BASE(txBytesSrc);
	INIT_SRC_STREAM(txBytesSrc,"net.device.txBytes",1,0,NULL,1000)
	SET_SELECTOR_STRING(txBytesSrc,0,"eth1")
}

static void destroyQueriesTXRXBytes(void) {
	freeOperator(GET_BASE(rxBytesSrc),0);
	freeOperator(GET_BASE(txBytesSrc),0);
}
