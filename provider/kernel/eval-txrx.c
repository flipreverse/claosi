static EventStream_t rxStreamBase, txStreamBase;
static Select_t rxSelectPacket, txSelectPacket;
static Filter_t rxFilterData, txFilterData;
static Predicate_t rxPredData, txPredData;
static Element_t rxElemPacket, txElemPacket;
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
	if (useRelayFS) {
		relay_write(relayfsOutput,&sample,sizeof(sample));
	}

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
	if (useRelayFS) {
		relay_write(relayfsOutput,&sample,sizeof(sample));
	}

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
	INIT_EVT_STREAM(rxStreamBase,"net.device.onRx",1,0,GET_BASE(rxFilterData))
	SET_SELECTOR_STRING(rxStreamBase,0,devName)
	INIT_FILTER(rxFilterData,GET_BASE(rxSelectPacket),1)
	ADD_PREDICATE(rxFilterData,0,rxPredData)
	SET_PREDICATE(rxPredData,GEQ, OP_STREAM, "net.packetType.dataLength", OP_POD, "1000")
	INIT_SELECT(rxSelectPacket,NULL,1)
	ADD_ELEMENT(rxSelectPacket,0,rxElemPacket,"net.packetType")

	initQuery(&queryTXBase);
	queryTXBase.onQueryCompleted = printResultTx;
	queryTXBase.root = GET_BASE(txStreamBase);
	INIT_EVT_STREAM(txStreamBase,"net.device.onTx",1,0,GET_BASE(txFilterData))
	SET_SELECTOR_STRING(txStreamBase,0,devName)
	INIT_FILTER(txFilterData,GET_BASE(txSelectPacket),1)
	ADD_PREDICATE(txFilterData,0,txPredData)
	SET_PREDICATE(txPredData,GEQ, OP_STREAM, "net.packetType.dataLength", OP_POD, "10")
	INIT_SELECT(txSelectPacket,NULL,1)
	ADD_ELEMENT(txSelectPacket,0,txElemPacket,"net.packetType")
}

static void destroyQueriesTXRX(void) {
	freeOperator(GET_BASE(rxStreamBase),0);
	freeOperator(GET_BASE(txStreamBase),0);
}
