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
static const char *fifoPath = FIFO_PATH;
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
	void *curDL = NULL;
	int read = 0, ret = 0, argc = 0;
	LoadedProvider_t *curProv = NULL, *tmpProv = NULL;
	int (*onLoadFn)(int argc, char *argv[]);
	char *line = NULL, *cmd, *temp, **argv;
	size_t lineSize = 0, argvSize = 10;

	while (1) {
		cmdFifo = fopen(fifoPath,"r");
		if (cmdFifo == NULL) {
			if (errno == EINTR) {
				continue;
			}
			ERR_MSG("Cannot open FIFO (%s): %s\n",FIFO_PATH,strerror(errno));
			pthread_exit(0);
		}
		read = getline(&line, &lineSize, cmdFifo);
		if (read == -1) {
			ERR_MSG("Cannot read from FIFO   (%s): %s\n",FIFO_PATH,strerror(errno));
			fclose(cmdFifo);
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
		}
		// Remove the newline. Getline copies the delimiter, i.e., \n, to the buffer.
		line[read - 1] = '\0';
		// Extract the command
		cmd = strtok(line, " ");
		if (cmd == NULL) {
			ERR_MSG("Cannot read cmd from line\n");
			fclose(cmdFifo);
			continue;
		}
		/*
		 * Create our own version of *argv[] that will be passed to the loaded library.
		 * Allocate space for one more pointer to accommodate for the terminating NULL pointer.
		 */
		argv = ALLOC(sizeof(*argv) * (argvSize + 1));
		if (argv == NULL) {
			ERR_MSG("Cannot allocate argv\n");
			fclose(cmdFifo);
			continue;
		}
		argc = 0;
		// Now store a pointer to each single token in the argv array
		while ((temp = strtok(NULL, " ")) != NULL) {
			argv[argc] = temp;
			argc++;
			if (argc >= argvSize) {
				argvSize += 5;
				DEBUG_MSG(2, "argv is too small. Resizing to %lu\n", argvSize);
				argv = realloc(argv, sizeof(*argv) * (argvSize + 1));
				if (argv == NULL) {
					ERR_MSG("Cannot realloc argv\n");
					break;
				}
			}
		}
		// Abort further processing, because realloc failed.
		if (argv == NULL) {
			continue;
		}
		/*
		 * To allow a provider to use getopt(), the very last argument of *our*
		 * argv must be a NULL pointer (1).
		 * 
		 * (1) https://en.cppreference.com/w/cpp/language/main_function
		 */
		argv[argc] = NULL;
		// Process the command
		if (strcmp(cmd,"add") == 0) {
			if (argc < 1 ) {
				ERR_MSG("Not enough arguments for cmd add\n");
				continue;
			}
			// Check if this provider has already been loaded....
			ret = 0;
			LIST_FOREACH(curProv, &providerList, listEntry) {
				if (strcmp(curProv->libPath, argv[0]) == 0) {
					ret = 1;
					break;
				}
			}
			if (ret) {
				ERR_MSG("Provider %s is already loaded\n", argv[0]);
				continue;
			}
			// No it is not present. Load it...
			curDL = dlopen(argv[0],RTLD_NOW);
			if (curDL == NULL) {
				ERR_MSG("Error loading dynamic library '%s': %s\n", argv[0], dlerror());
				continue;
			}
			// Try to resolve the entry function
			onLoadFn = dlsym(curDL,"onLoad");
			if (onLoadFn == NULL) {
				ERR_MSG("Error resolving symbol 'onLoad' from library '%s': %s\n", argv[0], dlerror());
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
			curProv->libPath = ALLOC(strlen(argv[0]) + 1);
			if (curProv->libPath == NULL) {
				ERR_MSG("Cannot allocate memory for library path\n");
				dlclose(curDL);
				FREE(curProv);
			}
			/*
			 * Initialize the provider...
			 * We explizitly do *not* skip the library path.
			 * To allow a provider to use getopt(), the first argument of *our*
			 * argv must be 'the name of the invoked program'(1).
			 * This is the library path in our case.
			 * 
			 * Since argv[0] is the program name and argv[argc] is a NULL pointer,
			 * we have successfully faked an argv[] as it would have been used for main().
			 * 
			 * (1) https://en.cppreference.com/w/cpp/language/main_function
			 */
			ret = onLoadFn(argc, argv);
			if (ret < 0) {
				ERR_MSG("Provider %s initialization failed: %d\n", argv[0], -ret);
				FREE(curProv->libPath);
				FREE(curProv);
				dlclose(curDL);
				continue;
			}
			strcpy(curProv->libPath, argv[0]);
			curProv->dlHandle = curDL;
			LIST_INSERT_HEAD(&providerList, curProv, listEntry);
		} else if (strcmp(cmd,"del") == 0) {
			if (argc < 1 ) {
				ERR_MSG("Not enough arguments for cmd add\n");
				continue;
			}
			ret = 0;
			LIST_FOREACH(curProv, &providerList, listEntry) {
				// The user has to provide the identical path to the shared library as for the add command
				if (strcmp(curProv->libPath, argv[0]) == 0) {
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
				ERR_MSG("No provider loaded with library path %s\n", argv[0]);
			}
		} else if (strcmp(cmd,"exit") == 0) {
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
		} else if (strcmp(cmd,"printdm") == 0) {
			ACQUIRE_READ_LOCK(slcLock);
			printDatamodel(SLC_DATA_MODEL);
			RELEASE_READ_LOCK(slcLock);
		} else {
			ERR_MSG("Unknown command!\n");
		}
		free(argv);
		fclose(cmdFifo);
	};
	if (line) {
		free(line);
	}

	pthread_exit(0);
	return NULL;
}

int main(int argc, const char *argv[]) {
	struct stat fifo_stat;
	LIST_INIT(&providerList);

	if (argc > 1) {
		fifoPath = argv[1];
	}
	// Does the fifo exist?
	if (stat(fifoPath,&fifo_stat) < 0) {
		// No. Create it.
		if (errno == ENOENT) {
			ERR_MSG("FIFO does not exist. Try to create it.\n");
			if (mkfifo(fifoPath,0777) < 0) {
				perror("mkfifo");
				return EXIT_FAILURE;
			}
			DEBUG_MSG(3,"Created fifo: %s\n",fifoPath);
		} else {
			ERR_MSG("Error probing for FIFO (%s): %s\n",fifoPath,strerror(errno));
			return EXIT_FAILURE;
		}
	} else {
		// The path exists, but is not a fifo --> Exit.
		if (!S_ISFIFO(fifo_stat.st_mode)) {
			ERR_MSG("%s is not a fifo.\n",fifoPath);
			return EXIT_FAILURE;
		}
	}

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
