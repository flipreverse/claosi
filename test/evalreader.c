#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <sys/poll.h>
#include <errno.h>
#include <evaluation.h>

#define BUFFER_SIZE SUBBUF_SIZE
#define OUTPUT_FILE_NAME "time-slc-net-kernel"
#define CHAR_BUFFER_SIZE 150

static pthread_t *threads;
static pthread_attr_t threadsAttr;

static const char *baseDirRead = NULL, *baseDirWrite = NULL, *outputFilePrefix = OUTPUT_FILE_NAME; 
static int running;

static void* readerThreadWork(void *data) {
	long cpu = (long)data, fdRead, fdWrite, bytesRead, toWrite;
	struct stat outputFileStat;
	char *path, *bufferRead, bufferWrite[CHAR_BUFFER_SIZE];
	Sample_t *curTS;
	struct pollfd pollFDs;
	sigset_t sigs;
	int fileExits = 1;

	sigemptyset(&sigs);
	sigaddset(&sigs,SIGUSR1);
	pthread_sigmask(SIG_BLOCK, &sigs, NULL);

	path = (char*)malloc(strlen(baseDirRead) + strlen( "/" RELAYFS_NAME) + 5);
	if (path == NULL) {
		perror("malloc path");
		pthread_exit(NULL);
	}
	sprintf(path,"%s/%s%ld",baseDirRead,RELAYFS_NAME,cpu);
	printf("%ld: Opening %s...\n",cpu,path);
	fdRead = open(path,O_RDONLY);
	if (fdRead < 0) {
		perror("open relayfs file");
		free(path);
		pthread_exit(NULL);
	}
	free(path);

	path = (char*)malloc(strlen(baseDirWrite) + strlen( "/" OUTPUT_FILE_NAME) + 5);
	sprintf(path,"%s/%s%ld.txt",baseDirWrite,outputFilePrefix,cpu);
	printf("%ld: Opening %s...\n",cpu,path);
	if (stat(path,&outputFileStat) < 0) {
		if (errno == ENOENT) {
			fileExits = 0;
		}
	}
	fdWrite = open(path,O_WRONLY|O_APPEND|O_CREAT,0744);
	if (fdWrite < 0) {
		perror("open output file");
		free(path);
		close(fdRead);
		pthread_exit(NULL);
	}
	free(path);

	bufferRead = (char*)malloc(BUFFER_SIZE);
	if (bufferRead == NULL) {
		perror("malloc buffer");
		close(fdRead);
		close(fdWrite);
		pthread_exit(NULL);
	}

	if (cpu == 0 && fileExits == 0) {
		toWrite = snprintf(bufferWrite,CHAR_BUFFER_SIZE,"ts1,ts2,ts3,ts4\n");
		if (write(fdWrite,bufferWrite,toWrite) < 0) {
			perror("write buffer");
		}
	}

	pollFDs.fd = fdRead;
	pollFDs.events = POLLIN;

	do {
		if (poll(&pollFDs,1,1000) < 0) {
			perror("ppoll");
			break;
		}

		bytesRead = read(fdRead,bufferRead,BUFFER_SIZE);
		if (bytesRead < 0) {
			perror("read buffer");
			break;
		}
		curTS = (Sample_t*)bufferRead;
		while (bytesRead > 0 && bytesRead >= sizeof(Sample_t)) {
			toWrite = snprintf(bufferWrite,CHAR_BUFFER_SIZE,"%llu,%llu,%llu,%llu\n",curTS->ts1,curTS->ts2,curTS->ts3,curTS->ts4);
			if (write(fdWrite,bufferWrite,toWrite) < 0) {
				perror("write buffer");
				break;
			}
			bytesRead -= sizeof(Sample_t);
			curTS += 1;
		}
	} while (running == 1);

	close(fdRead);
	close(fdWrite);
	free(bufferRead);
	pthread_exit(NULL);
	return NULL;
}

static void sigHandler(int signo) {
	running = 0;
}

int main(int argc, const char *argv[]) {
	long cpus, i;

	if (argc < 3) {
		printf("specify a base and output directory!\n");
		return EXIT_FAILURE;
	}
	baseDirRead = argv[1];
	baseDirWrite = argv[2];

	if (argc > 3) {
		outputFilePrefix = argv[3];
	}

	if (signal(SIGUSR1,sigHandler) < 0) {
		perror("registering signal handler\n");
		return EXIT_FAILURE;
	}

	cpus = sysconf(_SC_NPROCESSORS_ONLN);
	printf("Found %ld online cpus\n",cpus);

	threads = (pthread_t*)malloc(sizeof(pthread_t) * cpus);
	if (threads == NULL) {
		perror("malloc pthread_t");
		return EXIT_FAILURE;
	}
	pthread_attr_init(&threadsAttr);
	pthread_attr_setdetachstate(&threadsAttr,PTHREAD_CREATE_JOINABLE);

	running = 1;
	for (i = 0; i < cpus; i++) {
		pthread_create(&threads[i],&threadsAttr,readerThreadWork,(void*)i);
	}

	for (i = 0; i < cpus; i++) {
		pthread_join(threads[i],NULL);
//		printf("thread %d terminated\n",i);
	}

	pthread_exit(NULL);
}
