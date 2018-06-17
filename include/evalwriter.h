#ifndef __EVAL_WRITER_H__
#include <sys/prctl.h>
#include <sys/stat.h>
#define _GNU_SOURCE 
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <query.h>

#define isEmpty(var)		((var).read == (var).write)
#define isFull(var)			(((var).write + 1) % (var).size == (var).read)
#define SAMPLE_RING_BUFFER_SIZE 60
#define CHAR_BUFFER_SIZE 200
#define WRITE_THREAD_NAME "evalWriteThread"

typedef struct Sample {
	unsigned long long ts1, ts2, ts3, ts4;
} Sample_t;

typedef struct SampleRingbuffer {
	unsigned int size;
	unsigned int read;
	unsigned int write;
	Sample_t elements[SAMPLE_RING_BUFFER_SIZE];
} SampleRingbuffer_t;

static int useEvalReader = 0;
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

static void inline writeSample(Tupel_t *tupel, unsigned long long timeUS) {
	if (useEvalReader) {
		if (!isFull(sampleBuffer)) {
			sampleBuffer.elements[sampleBuffer.write].ts1 = tupel->timestamp;
#ifdef EVALUATION
			sampleBuffer.elements[sampleBuffer.write].ts2 = tupel->timestamp2;
			sampleBuffer.elements[sampleBuffer.write].ts3 = tupel->timestamp3;
#endif
			sampleBuffer.elements[sampleBuffer.write].ts4 = timeUS;
			sampleBuffer.write = (sampleBuffer.write + 1 == sampleBuffer.size ? 0 : sampleBuffer.write + 1);
		}
	}
}

static int setupEvalWriter(const char *outputFname) {
	int ret;

	if (useEvalReader) {
		outputFile = open(outputFname,O_WRONLY|O_CREAT|O_APPEND,0755);
		if (outputFile < 0) {
			ERR_MSG("Cannot open %s: %s\n",outputFname,strerror(errno));
			return -1;
		}
		INFO_MSG("Writing timestamps to %s\n", outputFname);

		sampleBuffer.read = 0;
		sampleBuffer.write = 0;
		sampleBuffer.size = SAMPLE_RING_BUFFER_SIZE;

		writeThreadRunning = 1;
		pthread_attr_init(&writeThreadAttr);
		pthread_attr_setdetachstate(&writeThreadAttr,PTHREAD_CREATE_JOINABLE);
		ret = pthread_create(&writeThread,&writeThreadAttr,writeThreadWork,NULL);
		if (ret != 0) {
			ERR_MSG("Cannot create %s: %s\n", WRITE_THREAD_NAME, strerror(ret));
			return -1;
		}
	}
	return 0;
}

static void destroyEvalWriter(void) {
	if (useEvalReader) {
		writeThreadRunning = 0;
		pthread_join(writeThread,NULL);
		pthread_attr_destroy(&writeThreadAttr);
		close(outputFile);
	}
}

#endif //__EVAL_WRITER_H__
