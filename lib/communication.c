#include <common.h>
#include <communication.h>
#include <output.h>
#include <liballoc.h>

#define isEmpty(var)		(var->read == var->write)
#define isFull(var)			((var->write + 1) % var->size == var->read)

void *sharedMemoryKernelBase = NULL;
void *sharedMemoryUserBase = NULL;
Ringbuffer_t *txBuffer = NULL;
Ringbuffer_t *rxBuffer = NULL;

static int remainingPages = BUFFER_PAGES;
static char *txMemory = NULL;
static int writeOps;

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

	rxBuffer = (Ringbuffer_t*)(sharedMemoryKernelBase + sizeof(Ringbuffer_t));
	rxBuffer->size = RING_BUFFER_SIZE;
	rxBuffer->read = 0;
	rxBuffer->write = 0;
	for (i = 0; i < RING_BUFFER_SIZE; i++) {
		rxBuffer->elements[i].type = MSG_EMPTY;
		rxBuffer->elements[i].addr = NULL;
	}

	DEBUG_MSG(2,"txBuffer=0x%p (size=%d), rxBuffer=0x%p (size=%d), txMemory=0x%p\n",txBuffer,txBuffer->size, rxBuffer, rxBuffer->size, txMemory);
#else
	txMemory = sharedMemoryUserBase + 1 * PAGE_SIZE + BUFFER_PAGES * PAGE_SIZE;

	rxBuffer = (Ringbuffer_t*)sharedMemoryUserBase;
	txBuffer = (Ringbuffer_t*)(sharedMemoryUserBase + sizeof(Ringbuffer_t));

	DEBUG_MSG(2,"txBuffer=%p (size=%d), rxBuffer=%p (size=%d), txMemory=%p\n",txBuffer,txBuffer->size, rxBuffer, rxBuffer->size, txMemory);
#endif
	remainingPages = BUFFER_PAGES;
	writeOps = 0;

	DEBUG_MSG(1,"Initialized ring buffer using %d elements\n",RING_BUFFER_SIZE);
}

LayerMessage_t* ringBufferReadBegin(Ringbuffer_t *ringBuffer) {
	LayerMessage_t *ret = NULL;

	if (!isEmpty(ringBuffer)) {
		ret = &ringBuffer->elements[ringBuffer->read];
	}
	return ret;
}

void ringBufferReadEnd(Ringbuffer_t *ringBuffer) {
	ringBuffer->elements[ringBuffer->write].type = MSG_EMPTY;
	ringBuffer->read = (ringBuffer->read + 1 == ringBuffer->size ? 0 : ringBuffer->read + 1);
}

int ringBufferWrite(Ringbuffer_t *ringBuffer, int type, char *addr) {
	int i = 0;

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

