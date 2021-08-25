#ifndef _QEUEU_H_
#define _QUEUE_H_

#include "tju_tcp.h"

#define MAX_QUEUE 1024

// struct tju_tcp_t;

typedef struct sock_node {
    tju_tcp_t* data;
    struct sock_node* next;
} sock_node;

typedef struct sock_queue {
    int size;
    sock_node* base;
} sock_queue;

void sockqueue_init(sock_queue** q);
int sockqueue_is_empty(sock_queue* q);
int sockqueue_size(sock_queue* q);
int sockqueue_push(sock_queue* q, tju_tcp_t* sock);
int sockqueue_pop(sock_queue* q, tju_tcp_t* sock);
int sockqueue_remove(sock_queue* q, tju_tcp_t* sock, int index);

#endif