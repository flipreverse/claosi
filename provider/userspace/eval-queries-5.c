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
#define OUTPUT_FILENAME "time-slc-5.txt"
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

static ObjectStream_t processObjFork;
static Join_t joinComm, joinStime;
static Predicate_t stimePredicate, commPredicate;
static Query_t queryForkJoin;

static int writeThreadRunning;
static pthread_t writeThread;
static pthread_attr_t writeThreadAttr;
static SampleRingbuffer_t sampleBuffer;
static char timestampBuffer[CHAR_BUFFER_SIZE];
static int outputFile;

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

static void printResultForkJoin(unsigned int id, Tupel_t *tupel) {
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

	if (!isFull(sampleBuffer)) {
		sampleBuffer.elements[sampleBuffer.write].ts1 = tupel->timestamp;
#ifdef EVALUATION
		sampleBuffer.elements[sampleBuffer.write].ts2 = tupel->timestamp2;
		sampleBuffer.elements[sampleBuffer.write].ts3 = tupel->timestamp3;
#endif
		sampleBuffer.elements[sampleBuffer.write].ts4 = timeUS;
		sampleBuffer.write = (sampleBuffer.write + 1 == sampleBuffer.size ? 0 : sampleBuffer.write + 1);
	}

	freeTupel(SLC_DATA_MODEL,tupel);
}

static void setupQueries(void) {
	initQuery(&queryForkJoin);
	queryForkJoin.onQueryCompleted = printResultForkJoin;
	queryForkJoin.root = GET_BASE(processObjFork);
	//queryForkJoin.next = & querySockets;
	INIT_OBJ_STREAM(processObjFork,"process.process",0,0,GET_BASE(joinStime),OBJECT_CREATE);
	INIT_JOIN(joinStime,"process.process.stime", GET_BASE(joinComm),1)
	ADD_PREDICATE(joinStime,0,stimePredicate)
	SET_PREDICATE(stimePredicate,EQUAL, OP_STREAM, "process.process", OP_JOIN, "process.process")
	INIT_JOIN(joinComm,"process.process.comm", NULL,1)
	ADD_PREDICATE(joinComm,0,commPredicate)
	SET_PREDICATE(commPredicate,EQUAL, OP_STREAM, "process.process", OP_JOIN, "process.process")
}

int onLoad(void) {
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

	ret = registerQuery(&queryForkJoin);
	if (ret < 0 ) {
		ERR_MSG("Register failed: %d\n",-ret);
		return -1;
	}
	DEBUG_MSG(1,"Registered eval net queries\n");

	return 0;
}

int onUnload(void) {
	int ret = 0;

	ret = unregisterQuery(&queryForkJoin);
	if (ret < 0 ) {
		ERR_MSG("Unregister eval net failed: %d\n",-ret);
		return -1;
	}

	writeThreadRunning = 0;
	pthread_join(writeThread,NULL);
	pthread_attr_destroy(&writeThreadAttr);
	close(outputFile);

	freeOperator(GET_BASE(processObjFork),0);
	DEBUG_MSG(1,"Unregistered eval net queries\n");

	return 0;
}
