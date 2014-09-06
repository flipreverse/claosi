#ifndef __EVALUATION_H__
#define __EVALUATION_H__

#define RELAYFS_DIR "eval"
#define RELAYFS_NAME "samples-out-"
#define SUBBUF_SIZE (sizeof(Sample_t) * 128)
#define N_SUBBUFS	5

typedef struct Sample {
	unsigned long long ts1, ts2, ts3, ts4;
} Sample_t;

#endif //__EVALUATION_H__
