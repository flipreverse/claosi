#define MSG_FMT(fmt) "[slc-evalremotequery] " fmt
#include <datamodel.h>
#include <query.h>
#include <api.h>
#define _GNU_SOURCE 
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/syscall.h>


#define PRINT_TUPLE
#undef PRINT_TUPLE

#define SAMPLE_RING_BUFFER_SIZE 60
#define CHAR_BUFFER_SIZE 200
#define OUTPUT_FILENAME "time-slc-net-userspace.txt"
#define WRITE_THREAD_NAME "evalWriteThread"

#define isEmpty(var)		((var).read == (var).write)
#define isFull(var)			(((var).write + 1) % (var).size == (var).read)

typedef struct Sample {
	unsigned long long ts1, ts2, ts3, ts4;
} Sample_t;

typedef struct SampleRingbuffer {
	unsigned int size;
	unsigned int read;
	unsigned int write;
	Sample_t elements[SAMPLE_RING_BUFFER_SIZE];
} SampleRingbuffer_t;

static EventStream_t rxStream, txStream;
static Predicate_t rxJoinProcessPredicateSocket, rxJoinProcessPredicatePID, txJoinProcessPredicate, txJoinProcessPredicatePID;
static Join_t rxJoinProcess, txJoinProcess;
static Query_t queryRX, queryTX;

static int writeThreadRunning;
static pthread_t writeThread;
static pthread_attr_t writeThreadAttr;
static SampleRingbuffer_t sampleBuffer;
static char timestampBuffer[CHAR_BUFFER_SIZE];
static int outputFile;
static unsigned long long nRx, nTx;

static void* writeThreadWork(void *data) {
	int cur = 0, written = 0;
	struct timespec sleepTime, remSleepTime;

	if (prctl(PR_SET_NAME,WRITE_THREAD_NAME,0,0,0) <0) {
		ERR_MSG("error:%s\n",strerror(errno));
	}
	PRINT_MSG("%s: tid=%ld\n",WRITE_THREAD_NAME,syscall(SYS_gettid));

	written = snprintf(timestampBuffer,CHAR_BUFFER_SIZE,"ts1,ts2,ts3,ts4\n");
	write(outputFile,timestampBuffer,written);

	sleepTime.tv_sec = 0;
	sleepTime.tv_nsec = 20000000;

	while (writeThreadRunning == 1) {
		while (!isEmpty(sampleBuffer)) {
			cur = sampleBuffer.read;
			written = snprintf(timestampBuffer,CHAR_BUFFER_SIZE,"%llu,%llu,%llu,%llu\n",sampleBuffer.elements[cur].ts1,sampleBuffer.elements[cur].ts2,sampleBuffer.elements[cur].ts3,sampleBuffer.elements[cur].ts4);
			write(outputFile,timestampBuffer,written);
			sampleBuffer.read = (sampleBuffer.read + 1 == sampleBuffer.size ? 0 : sampleBuffer.read + 1);
		}
		nanosleep(&sleepTime,&remSleepTime);
	}

	pthread_exit(NULL);
	return NULL;
}
static void printResultRx(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

#ifdef PRINT_TUPLE
	printf("Received packet on device %s. Received by process %d. Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"process.process"),getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	if (!isFull(sampleBuffer)) {
		sampleBuffer.elements[sampleBuffer.write].ts1 = tupel->timestamp;
#ifdef EVALUATION
		sampleBuffer.elements[sampleBuffer.write].ts2 = tupel->timestamp2;
		sampleBuffer.elements[sampleBuffer.write].ts3 = tupel->timestamp3;
#endif
		sampleBuffer.elements[sampleBuffer.write].ts4 = timeUS;
		sampleBuffer.write = (sampleBuffer.write + 1 == sampleBuffer.size ? 0 : sampleBuffer.write + 1);
	}
	nRx++;

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void printResultTx(unsigned int id, Tupel_t *tupel) {
#ifndef EVALUATION
	struct timeval time;
#endif
	unsigned long long timeUS = 0;

#ifdef EVALUATION
	timeUS = getCycles();
#else
	gettimeofday(&time,NULL);
	timeUS = (unsigned long long)time.tv_sec * (unsigned long long)USEC_PER_SEC + (unsigned long long)time.tv_usec;
#endif

#ifdef PRINT_TUPLE
	printf("Transmitted packet on device %s. Sent by process %d. Packet length=%d (itemLen=%d,tuple=%p,duration=%llu us)\n",getItemString(SLC_DATA_MODEL,tupel,"net.device"),getItemInt(SLC_DATA_MODEL,tupel,"process.process"),getItemInt(SLC_DATA_MODEL,tupel,"net.packetType.dataLength"),tupel->itemLen,tupel,timeUS - tupel->timestamp);
#endif

	if (!isFull(sampleBuffer)) {
		sampleBuffer.elements[sampleBuffer.write].ts1 = tupel->timestamp;
#ifdef EVALUATION
		sampleBuffer.elements[sampleBuffer.write].ts2 = tupel->timestamp2;
		sampleBuffer.elements[sampleBuffer.write].ts3 = tupel->timestamp3;
#endif
		sampleBuffer.elements[sampleBuffer.write].ts4 = timeUS;
		sampleBuffer.write = (sampleBuffer.write + 1 == sampleBuffer.size ? 0 : sampleBuffer.write + 1);
	}
	nTx++;

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryRX);
	queryRX.onQueryCompleted = printResultRx;
	queryRX.root = GET_BASE(rxStream);
	queryRX.next = &queryTX;
	INIT_EVT_STREAM(rxStream,"net.device.onRx",1,0,GET_BASE(rxJoinProcess))
	SET_SELECTOR_STRING(rxStream,0,"eth0")
	INIT_JOIN(rxJoinProcess,"process.process.sockets",NULL,2)
	ADD_PREDICATE(rxJoinProcess,0,rxJoinProcessPredicateSocket)
	SET_PREDICATE(rxJoinProcessPredicateSocket,EQUAL, OP_JOIN, "process.process.sockets", OP_STREAM, "net.packetType.socket")
	ADD_PREDICATE(rxJoinProcess,1,rxJoinProcessPredicatePID)
	SET_PREDICATE(rxJoinProcessPredicatePID,EQUAL, OP_JOIN, "process.process", OP_POD, "-1")

	initQuery(&queryTX);
	queryTX.onQueryCompleted = printResultTx;
	queryTX.root = GET_BASE(txStream);
	INIT_EVT_STREAM(txStream,"net.device.onTx",1,0,GET_BASE(txJoinProcess))
	SET_SELECTOR_STRING(txStream,0,"eth0")
	INIT_JOIN(txJoinProcess,"process.process.sockets",NULL,2)
	ADD_PREDICATE(txJoinProcess,0,txJoinProcessPredicate)
	SET_PREDICATE(txJoinProcessPredicate,EQUAL, OP_JOIN, "process.process.sockets", OP_STREAM, "net.packetType.socket")
	ADD_PREDICATE(txJoinProcess,1,txJoinProcessPredicatePID)
	SET_PREDICATE(txJoinProcessPredicatePID,EQUAL, OP_JOIN, "process.process", OP_POD, "-1")
}

int onLoad(int argc, char *argv[]) {
	int ret = 0;
	setupQueries();

	outputFile = open(OUTPUT_FILENAME,O_WRONLY|O_CREAT|O_APPEND,0755);
	if (outputFile < 0) {
		ERR_MSG("Cannot open %s: %s\n",OUTPUT_FILENAME,strerror(errno));
		return -1;
	}

	sampleBuffer.read = 0;
	sampleBuffer.write = 0;
	sampleBuffer.size = SAMPLE_RING_BUFFER_SIZE;

	writeThreadRunning = 1;
	pthread_attr_init(&writeThreadAttr);
	pthread_attr_setdetachstate(&writeThreadAttr,PTHREAD_CREATE_JOINABLE);
	pthread_create(&writeThread,&writeThreadAttr,writeThreadWork,NULL);

	ret = registerQuery(&queryRX);
	if (ret < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		return -1;
	}
	INFO_MSG("Registered eval net queries\n");

	return 0;
}

int onUnload(void) {
	int ret = 0;

	ret = unregisterQuery(&queryRX);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval net failed: %d\n",-ret);
		return -1;
	}

	writeThreadRunning = 0;
	pthread_join(writeThread,NULL);
	pthread_attr_destroy(&writeThreadAttr);
	close(outputFile);

	INFO_MSG("nRx=%llu, nTx=%llu\n",nRx,nTx);
	freeOperator(GET_BASE(rxStream),0);
	freeOperator(GET_BASE(txStream),0);
	INFO_MSG("Unregistered eval net queries\n");

	return 0;
}
