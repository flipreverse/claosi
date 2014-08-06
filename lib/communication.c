#include <common.h>
#include <communication.h>
#include <output.h>
#include <liballoc.h>

#define isEmpty(var)		(var->read == var->write)
#define isFull(var)			((var->write + 1) % var->size == var->read)

/**
 * Start address of the shared memory within the kernel
 */
void *sharedMemoryKernelBase = NULL;
/**
 * Start address of the shared memory within the userspace (a.k.a return value of mmap())
 */
void *sharedMemoryUserBase = NULL;
Ringbuffer_t *txBuffer = NULL;
Ringbuffer_t *rxBuffer = NULL;
/**
 * Determines the number of remaining pages in the txMemory.
 * Used by liballoc.c
 */
static int remainingPages = BUFFER_PAGES;
static char *txMemory = NULL;
/**
 * Number of writes to a ringbuffer.
 * Each layer has just on txBuffer. Therefore, only one static writeOps is needed.
 */
static int writeOps;
unsigned int *globalQueryID;

/**
 * Initialize the ring buffer according to the current layer.
 * It is up to the caller to set up sharedMemoryUserBase and sharedMemoryKernelBase.
 */
void ringBufferInit(void) {
#ifdef __KERNEL__
	int i = 0;
	txMemory = sharedMemoryKernelBase + 1 * PAGE_SIZE;

	txBuffer = (Ringbuffer_t*)sharedMemoryKernelBase;
	txBuffer->size = RING_BUFFER_SIZE;
	txBuffer->read = 0;
	txBuffer->write = 0;
	for (i = 0; i < RING_BUFFER_SIZE; i++) {
		txBuffer->elements[i].type = MSG_EMPTY;
		txBuffer->elements[i].addr = NULL;
	}
	/*
	 * We set up the rxBuffer (a.k.a userspace txBuffer) as well.
	 * Since the core module is loaded a kernelthread is running and reads from the receive buffer.
	 * Hence, it has to be initialized.
	 */
	rxBuffer = (Ringbuffer_t*)(sharedMemoryKernelBase + sizeof(Ringbuffer_t));
	rxBuffer->size = RING_BUFFER_SIZE;
	rxBuffer->read = 0;
	rxBuffer->write = 0;
	for (i = 0; i < RING_BUFFER_SIZE; i++) {
		rxBuffer->elements[i].type = MSG_EMPTY;
		rxBuffer->elements[i].addr = NULL;
	}

	globalQueryID = (int*)(sharedMemoryKernelBase + sizeof(Ringbuffer_t) * 2);
	*globalQueryID = 1;

	DEBUG_MSG(2,"txBuffer=0x%p (size=%d), rxBuffer=0x%p (size=%d), txMemory=0x%p\n",txBuffer,txBuffer->size, rxBuffer, rxBuffer->size, txMemory);
#else
	txMemory = sharedMemoryUserBase + 1 * PAGE_SIZE + BUFFER_PAGES * PAGE_SIZE;
	// Swap the position of rx- and txBuffer in contrast to its location in the kernel
	rxBuffer = (Ringbuffer_t*)sharedMemoryUserBase;
	txBuffer = (Ringbuffer_t*)(sharedMemoryUserBase + sizeof(Ringbuffer_t));

	DEBUG_MSG(2,"txBuffer=%p (size=%d), rxBuffer=%p (size=%d), txMemory=%p\n",txBuffer,txBuffer->size, rxBuffer, rxBuffer->size, txMemory);
#endif
	remainingPages = BUFFER_PAGES;
	writeOps = 0;

	DEBUG_MSG(2,"Initialized ring buffer using %d elements\n",RING_BUFFER_SIZE);
}
/**
 * Tries to read from {@link ringBuffer}. If it is empty, NULL will be returned.
 * The read index will *not* be updated.
 * @param ringBuffer a pointer to the buffer to read from
 * @return a pointer to the next element. Or null, if there is none.
 */
LayerMessage_t* ringBufferReadBegin(Ringbuffer_t *ringBuffer) {
	LayerMessage_t *ret = NULL;

	if (ringBuffer == NULL) {
		return NULL;
	}

	if (!isEmpty(ringBuffer)) {
		ret = &ringBuffer->elements[ringBuffer->read];
	}
	return ret;
}
/**
 * Empties the current message and increments the read index.
 * @param ringBuffer a pointer to the ringbuffer to operate on
 */
void ringBufferReadEnd(Ringbuffer_t *ringBuffer) {
	if (ringBuffer == NULL) {
		return;
	}

	ringBuffer->elements[ringBuffer->read].type = MSG_EMPTY;
	ringBuffer->read = (ringBuffer->read + 1 == ringBuffer->size ? 0 : ringBuffer->read + 1);
}
/**
 * Tries to write a message with {@link type} and {@link addr} to the ringbuffer.
 * If it is full, it aborts and returns -1.
 * If writeOps exceeds RING_BUFFER_SIZE / 2, it goes through all unused elements and frees memory used by previous messages.
 * @param ringBuffer a pointer to the ringBuffer to write to
 * @param type the message type
 * @param addr an address pointing to the messages payload
 * @return 0 on success. -1 on failure.
 */
int ringBufferWrite(Ringbuffer_t *ringBuffer, int type, char *addr) {
	int i = 0;
	if (ringBuffer == NULL) {
		return -1;
	}

	if (isFull(ringBuffer)) {
		return -1;
	} else {
		if (writeOps >= (RING_BUFFER_SIZE / 2)) {
			DEBUG_MSG(1,"Reached %d write operations. Looking for unfreed memory...\n",writeOps);
			writeOps = 0;
			for (i = ringBuffer->write; i < ringBuffer->read;) {
				if (ringBuffer->elements[i].type == MSG_EMPTY) {
					if (ringBuffer->elements[i].addr != NULL) {
						DEBUG_MSG(1,"Freeing memory of unused ringbuffer element %d: %p\n",i,ringBuffer->elements[i].addr);
						slcfree(ringBuffer->elements[i].addr);
						ringBuffer->elements[i].addr = NULL;
					}
				} else {
					ERR_MSG("Ringbuffer element is not marked as empty. Although it should be. ringbuffer=0x%lx, element=%d\n",(unsigned long)ringBuffer,i);
				}
				i = (i + 1 == ringBuffer->size ? 0 : i + 1);
			}
		}
		DEBUG_MSG(3,"Wrote message with type 0x%x and addr %p at %d\n",type,addr,ringBuffer->write);
		ringBuffer->elements[ringBuffer->write].type = type;
		ringBuffer->elements[ringBuffer->write].addr = addr;
		writeOps++;
		ringBuffer->write = (ringBuffer->write + 1 == ringBuffer->size ? 0 : ringBuffer->write + 1);
		return 0;
	}
}

void* liballoc_alloc(size_t pages) {
	void *ret = NULL;

	if (remainingPages - (int)pages >= 0) {
		ret = txMemory + PAGE_SIZE * (BUFFER_PAGES - remainingPages);
		remainingPages -= pages;
	}

	return ret;
}

