#ifndef _QEUEU_H_
#define _QUEUE_H_
#include "tju_tcp.h"

#define MAX_QUEUE 1024


typedef struct sock_node {
    tju_tcp_t* data;
    sock_node* next;
} sock_node;

typedef struct sock_queue {
    int size;
    sock_node* base;
} sock_queue;

void queue_init(sock_queue* q);
int is_empty(sock_queue* q);
int size(sock_queue* q);
int push(sock_queue* q, tju_tcp_t* sock);
int pop(sock_queue* q, tju_tcp_t* sock);
int queue_remove(sock_queue* q, tju_tcp_t* sock, int index);

#endif