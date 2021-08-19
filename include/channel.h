#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <mqueue.h>
#include "global.h"


typedef struct list {
    char* remote_ip;
    char* data;
    struct list* next;
    struct list* prev;
} list;

typedef struct channel_t {
    pthread_mutex_t lock;
    int initialized;
    struct list* list;
} channel_t;


void chan_init();
int chan_send(char* buf, int len, char* ip);
int chan_recv(char* buf, int len, char* ip);

#endif