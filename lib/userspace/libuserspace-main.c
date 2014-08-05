#include <api.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/queue.h>
#include <common.h>

#define FIFO_PATH "./slc-userspace-input"
#define BUFFER_SIZE 	128
static char pathBuffer[BUFFER_SIZE];
static char cmdBuffer[10];
static pthread_t fifoWorkThread;
static pthread_attr_t fifoWorkThreadAttr;

int initLayer(void);
void exitLayer(void);
/**
 * Represents a loaded userspace proivder.
 * Immediately after a user sends an add through the fifo the shared library will be loaded.
 * If all symbols can be resolved, the dl handle and the library path will be stored in an
 * instance of LoadedProvider_t which is enqueue in the provider list.
 */
typedef struct LoadedProvider {
	LIST_ENTRY(LoadedProvider) listEntry;
	/**
	 * The handler for the shared library returned by dlopen().
	 */
	void *dlHandle;
	/**
	 * The path of the shared library.
	 * Just needed to identify a certain library, if the user wants to delete it using 'del <path>'
	 */
	char *libPath;
} LoadedProvider_t;
/**
 * The head of the provider list (aka linked-list)
 */
LIST_HEAD(ProviderListHead,LoadedProvider) providerList;

static void unloadProvider(LoadedProvider_t *curProv) {
	int (*onUnloadFn)(void);
	int ret = 0;

	onUnloadFn = dlsym(curProv->dlHandle,"onUnload");
	// No unloadFn symbol! Refuse to unload the library.
	if (onUnloadFn == NULL) {
		ERR_MSG("Cannot find symbol 'onUnload'. Refusing to unload provider %s.\n",curProv->libPath);
		return;
	}
	ret = onUnloadFn();
	if (ret < 0) {
		ERR_MSG("Provider %s destruction failed: %d\n",curProv->libPath,-ret);
		return;
	}
	dlclose(curProv->dlHandle);
}

static void* fifoWork(void *data) {
	FILE *cmdFifo;
	struct stat fifo_stat;
	void *curDL = NULL;
	int read = 0, ret = 0;
	LoadedProvider_t *curProv = NULL, *tmpProv = NULL;
	int (*onLoadFn)(void);

	// Does the fifo exist?
	if (stat(FIFO_PATH,&fifo_stat) < 0) {
		// No. Create it.
		if (errno == ENOENT) {
			ERR_MSG("FIFO does not exist. Try to create it.\n");
			if (mkfifo(FIFO_PATH,0777) < 0) {
				perror("mkfifo");
				pthread_exit(0);
			}
			DEBUG_MSG(3,"Created fifo: %s\n",FIFO_PATH);
		} else {
			ERR_MSG("Error probing for FIFO (%s): %s\n",FIFO_PATH,strerror(errno));
			pthread_exit(0);
		}
	} else {
		// The path exists, but is not a fifo --> Exit.
		if (!S_ISFIFO(fifo_stat.st_mode)) {
			ERR_MSG("%s is not a fifo.\n",FIFO_PATH);
			pthread_exit(0);
		}
	}
	while (1) {
		cmdFifo = fopen(FIFO_PATH,"r");
		if (cmdFifo == NULL) {
			if (errno == EINTR) {
				continue;
			}
			ERR_MSG("Cannot open FIFO (%s): %s\n",FIFO_PATH,strerror(errno));
			pthread_exit(0);
		}
		// A command from the fifo: <command> [argument]
		read = fscanf(cmdFifo,"%10s %127s\n",cmdBuffer,pathBuffer);
		// At least one token is necessary
		if (read == 0 || read == EOF) {
			ERR_MSG("Cannot read from FIFO (%s): %s\n",FIFO_PATH,strerror(errno));
			fclose(cmdFifo);
			pthread_exit(0);
		}
		fclose(cmdFifo);
		if (strcmp(cmdBuffer,"add") == 0) {
			curDL = dlopen(pathBuffer,RTLD_NOW);
			if (curDL == NULL) {
				ERR_MSG("Error loading dynamic library '%s': %s\n",pathBuffer,dlerror());
				continue;
			}
			// Try to resolve the entry function
			onLoadFn = dlsym(curDL,"onLoad");
			if (onLoadFn == NULL) {
				ERR_MSG("Error resolving symbol 'onLoad' from library '%s': %s\n",pathBuffer,dlerror());
				dlclose(curDL);
				continue;
			}
			// Found it. Allocate space for an entry in the provider list
			curProv = ALLOC(sizeof(LoadedProvider_t));
			if (curProv == NULL) {
				ERR_MSG("Cannot allocate memory for LoadedProvider_t\n");
				dlclose(curDL);
				continue;
			}
			curProv->libPath = ALLOC(strlen(pathBuffer) + 1);
			if (curProv->libPath == NULL) {
				ERR_MSG("Cannot allocate memory for library path\n");
				dlclose(curDL);
				FREE(curProv);
			}
			// Initialize the provider
			ret = onLoadFn();
			if (ret < 0) {
				ERR_MSG("Provider %s initialization failed: %d\n",pathBuffer,-ret);
				FREE(curProv->libPath);
				FREE(curProv);
				dlclose(curDL);
				continue;
			}
			strcpy(curProv->libPath,pathBuffer);
			curProv->dlHandle = curDL;
			LIST_INSERT_HEAD(&providerList, curProv, listEntry);
		} else if (strcmp(cmdBuffer,"del") == 0) {
			ret = 0;
			LIST_FOREACH(curProv, &providerList, listEntry) {
				// The user has to provide the identical path to the shared library as for the add command
				if (strcmp(curProv->libPath,pathBuffer) == 0) {
					ret = 1;
					break;
				}
			}
			if (ret) {
				unloadProvider(curProv);
				LIST_REMOVE(curProv,listEntry);
				FREE(curProv->libPath);
				FREE(curProv);
			} else {
				ERR_MSG("No provider loaded with library path %s\n",pathBuffer);
			}
		} else if (strcmp(cmdBuffer,"exit") == 0) {
			/*
			 * Gracefully shutdown the userspace layer
			 * 
			 */
			for (curProv = LIST_FIRST(&providerList); curProv != NULL; curProv = tmpProv) {
				tmpProv = LIST_NEXT(curProv,listEntry);
				unloadProvider(curProv);
				LIST_REMOVE(curProv,listEntry);
				FREE(curProv->libPath);
				FREE(curProv);
			}
			break;
		} else if (strcmp(cmdBuffer,"printdm") == 0) {
			ACQUIRE_READ_LOCK(slcLock);
			printDatamodel(SLC_DATA_MODEL);
			RELEASE_READ_LOCK(slcLock);
		} else {
			ERR_MSG("Unknown command!\n");
		}
	};

	pthread_exit(0);
	return NULL;
}

int main(int argc, const char *argv[]) {
	LIST_INIT(&providerList);

	// First, bring up the layer-specific stuff
	if (initLayer() < 0) {
		return EXIT_FAILURE;
	}
	// Second, initialize the common slc stuff
	initSLC();

	pthread_attr_init(&fifoWorkThreadAttr);
	pthread_attr_setdetachstate(&fifoWorkThreadAttr, PTHREAD_CREATE_JOINABLE);
	if (pthread_create(&fifoWorkThread,&fifoWorkThreadAttr,fifoWork,NULL) < 0) {
		ERR_MSG("Cannot create fifoWorkThread: %s\n",strerror(errno));
		return EXIT_FAILURE;
	}
	// Wait for the fifo thread. It only terminates, if the user sends us an exit command.
	pthread_join(fifoWorkThread,NULL);
	pthread_attr_destroy(&fifoWorkThreadAttr);

	destroySLC();
	exitLayer();

	return EXIT_SUCCESS;
}
