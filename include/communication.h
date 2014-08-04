#ifndef __COMMUNICATION_H__
#define __COMMUNICATION_H__

#define BUFFER_PAGES			32
#define NUM_PAGES				(2 * BUFFER_PAGES + 1)
#define RING_BUFFER_SIZE		20

enum LayerMessageType {
	MSG_EMPTY				=	0x1,
	MSG_DM_ADD,
	MSG_DM_DEL,
	MSG_TUPLE
};

typedef struct LayerMessage {
	unsigned int type;
	char *addr;
} LayerMessage_t;

typedef struct Ringbuffer {
	unsigned int size;
	unsigned int read;
	unsigned int write;
	LayerMessage_t elements[RING_BUFFER_SIZE];
} Ringbuffer_t;

extern void *sharedMemoryKernelBase;
extern void *sharedMemoryUserBase;
extern Ringbuffer_t *txBuffer;
extern Ringbuffer_t *rxBuffer;

void ringBufferInit(void);
LayerMessage_t* ringBufferReadBegin(Ringbuffer_t *ringBuffer);
void ringBufferReadEnd(Ringbuffer_t *ringBuffer);
int ringBufferWrite(Ringbuffer_t *ringBuffer, int type, char *addr);


#endif // __COMMUNICATION_H__
