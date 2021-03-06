static EventStream_t rxStreamJoin, txStreamJoin;
static Predicate_t rxJoinProcessPredicateSocket, rxJoinProcessPredicatePID, txJoinProcessPredicate, txJoinProcessPredicatePID;
static Join_t rxJoinProcess, txJoinProcess;
static Query_t queryRXJoin, queryTXJoin;

static void printResultRxJoin(unsigned int id, Tupel_t *tupel) {
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

static void printResultTxJoin(unsigned int id, Tupel_t *tupel) {
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

static void setupQueriesTXRXJoin(Query_t **query) {
	Query_t *temp;

	if (*query == NULL) {
		*query = &queryRXJoin;
	} else {
		temp = *query;
		while (temp->next != NULL) {
			temp = temp->next;
		}
		temp->next = &queryRXJoin;
	}

	initQuery(&queryRXJoin);
	queryRXJoin.onQueryCompleted = printResultRxJoin;
	queryRXJoin.root = GET_BASE(rxStreamJoin);
	queryRXJoin.next = &queryTXJoin;
	INIT_EVT_STREAM(rxStreamJoin,"net.device.onRx",1,0,GET_BASE(rxJoinProcess))
	SET_SELECTOR_STRING(rxStreamJoin,0,devName)
	INIT_JOIN(rxJoinProcess,"process.process.sockets",NULL,2)
	ADD_PREDICATE(rxJoinProcess,0,rxJoinProcessPredicateSocket)
	SET_PREDICATE(rxJoinProcessPredicateSocket,EQUAL, OP_JOIN, "process.process.sockets", OP_STREAM, "net.packetType.socket")
	ADD_PREDICATE(rxJoinProcess,1,rxJoinProcessPredicatePID)
	SET_PREDICATE(rxJoinProcessPredicatePID,EQUAL, OP_JOIN, "process.process", OP_POD, "-1")

	initQuery(&queryTXJoin);
	queryTXJoin.onQueryCompleted = printResultTxJoin;
	queryTXJoin.root = GET_BASE(txStreamJoin);
	INIT_EVT_STREAM(txStreamJoin,"net.device.onTx",1,0,GET_BASE(txJoinProcess))
	SET_SELECTOR_STRING(txStreamJoin,0,devName)
	INIT_JOIN(txJoinProcess,"process.process.sockets",NULL,2)
	ADD_PREDICATE(txJoinProcess,0,txJoinProcessPredicate)
	SET_PREDICATE(txJoinProcessPredicate,EQUAL, OP_JOIN, "process.process.sockets", OP_STREAM, "net.packetType.socket")
	ADD_PREDICATE(txJoinProcess,1,txJoinProcessPredicatePID)
	SET_PREDICATE(txJoinProcessPredicatePID,EQUAL, OP_JOIN, "process.process", OP_POD, "-1")
}

static void destroyQueriesTXRXJoin(void) {
	freeOperator(GET_BASE(rxStreamJoin),0);
	freeOperator(GET_BASE(txStreamJoin),0);
}
