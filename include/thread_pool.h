#include "global.h"

typedef struct {
    pthread_t* thread;
    int allocated;
} handler;

typedef struct {
    pthread_mutex_t lock;
    handler* table[THREAD_POOL_SIZE];
} thread_pool;

extern thread_pool* pool;

int apply();
void destory(int index);