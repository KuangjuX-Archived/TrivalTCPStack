#include "sockqueue.h"

void sockqueue_init(sock_queue** q) {
    *q = (sock_queue*)malloc(sizeof(sock_queue));
    (*q)->size = 0;
    (*q)->base = NULL;
}

int sockqueue_size(sock_queue* q) {
    return q->size;
}

int sockqueue_is_empty(sock_queue* q) {
    return q->size == 0;
}

int sockqueue_push(sock_queue* q, tju_tcp_t* sock) {
    if (q->size == MAX_QUEUE) {
        return -1;
    }

    sock_node* node = q->base;
    if(node == NULL) {
        node = (sock_node*)malloc(sizeof(sock_node));
        node->data = sock;
        node->next = NULL;

        q->base = node;
        q->size += 1;
        return 0;
    }

    // 找到最后一个节点
    while(node->next != NULL) {
        node = node->next;
    }
    // 分配一个新的节点
    sock_node* new_node = (sock_node*)malloc(sizeof(sock_node));
    new_node->data = sock;
    new_node->next = NULL;
    node->next = new_node;

    q->size += 1;
    printf("the size of queue is %d.\n", q->size);

    // 返回当前节点的id
    return q->size - 1;
}

int sockqueue_pop(sock_queue* q, tju_tcp_t* sock) {
    if(sockqueue_is_empty(q)) {
        // 队列为空，返回-1
        return -1;
    }
    // 返回并pop顶部节点
    sock_node* node = q->base;
    q->base = node->next;
    *sock = *(node->data);
    free(node);
    q->size -= 1;
    return 0;
}


int sockqueue_remove(sock_queue* q, tju_tcp_t* sock, int index) {
    if(sockqueue_is_empty(q)) {
        return -1;
    }
    sock_node* node = q->base;

     if(index == 0) {
        *sock = *(node->data);
        free(node);
        q->base = NULL;
        return 0;
    }

    for(int i = 1; i < index; i++) {
        node = node->next;
        if(node == NULL) {
            return -1;
        }
    }
    // 找到目标节点的前一个节点
    sock_node* prev = node;
    node = node->next;
    if(node == NULL) {
        return -1;
    }
    // 去除节点，更新链表
    prev->next = node->next;
    *sock = *(node->data);
    free(node);
    q->size -= 1;

    return 0;
}

