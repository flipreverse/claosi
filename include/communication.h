#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#define BUFFER_PAGES			32
#define NUM_PAGES				(2 * BUFFER_PAGES + 1)
#define RING_BUFFER_SIZE		40

enum LayerMessageType {
	MSG_EMPTY				=	0x1,
	MSG_DM_ADD,
	MSG_DM_DEL,
	MSG_DM_SNAPSHOT,
	MSG_QUERY_ADD,
	MSG_QUERY_DEL,
	MSG_QUERY_CONTINUE
};
/**
 * Used to send messages between layers.
 */
typedef struct LayerMessage {
	/**
	 * Idicates the type of message. Its initialize value is MSG_EMPTY.
	 */
	unsigned int type;
	/**
	 * Points to a memory location within the sender-side txMemory.
	 * Its value should be between txMemory and txMemory + PAGE_SIZE * BUFFER_PAGES.
	 */
	char *addr;
} LayerMessage_t;

typedef struct Ringbuffer {
	/**
	 * Maximum number of messages
	 * The real maximum is size - 1 otherwise it is not possible to distinguish bufferFull() and bufferEmpty().
	 * See http://en.wikipedia.org/wiki/Circular_buffer#Always_Keep_One_Slot_Open
	 */
	unsigned int size;
	/**
	 * Index of the next message which should be read
	 */
	unsigned int read;
	/**
	 * Index at which to write a new element
	 */
	unsigned int write;
	/**
	 * Array of messages which are the actual ringbuffer
	 */
	LayerMessage_t elements[RING_BUFFER_SIZE];
} Ringbuffer_t;

extern void *sharedMemoryKernelBase;
extern void *sharedMemoryUserBase;
extern Ringbuffer_t *txBuffer;
extern Ringbuffer_t *rxBuffer;
extern unsigned int *globalQueryID;

void ringBufferInit(void);
LayerMessage_t* ringBufferReadBegin(Ringbuffer_t *ringBuffer);
void ringBufferReadEnd(Ringbuffer_t *ringBuffer);
int ringBufferWrite(Ringbuffer_t *ringBuffer, int type, char *addr);


#endif // __COMMUNICATION_H__
