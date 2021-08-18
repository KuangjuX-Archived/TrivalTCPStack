#ifndef _CHANNEL_H_
#define _CHANNEL_H_

#include <mqueue.h>
#include "global.h"

typedef struct {
    char* remote_ip;
    char* data;
    list* next;
    list* prev;
} list;

typedef struct {
    pthread_mutex_t lock;
    int initialized;
    list* list;
} channel_t;


void chan_init();
int chan_send(char* buf, int len, char* ip);
int chan_recv(char* buf, int len, char* ip);

#endif