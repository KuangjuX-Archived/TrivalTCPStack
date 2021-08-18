#include "channel.h"


channel_t* channels;

void chan_init() {
    pthread_mutex_init(&channels->lock, NULL);
    channels->initialized = TRUE;
    channels->list->data = NULL;
    channels->list->remote_ip = NULL;
    channels->list->next = NULL;
    channels->list->prev = NULL;
}

int chan_send(char* buf, int len, char* ip) {
    if (!channels->initialized) {
        printf("channels should be initialized");
        return 0;
    }
    pthread_mutex_lock(&channels->lock);
    list* node = channels->list;

    if (node->remote_ip != NULL) {
        while (node->next != NULL) {
            node = node->next;
        }
    }

    list* empty_node = (list*)malloc(sizeof(list));
    char* data = (char*)malloc(len);
    strcpy(data, buf);
    char* remote_ip = (char*)malloc(32);
    strcpy(remote_ip, ip);
    
    empty_node->data = data;
    empty_node->remote_ip = remote_ip;
    empty_node->next = NULL;
    empty_node->prev = node;
    node->next = empty_node;

    pthread_mutex_unlock(&channels->lock);
    return 1;
}

int chan_recv(char* buf, int len, char* ip) {
    if (!channels->initialized) {
        printf("channels should be initialized");
    }
    pthread_mutex_lock(&channels->lock);
    list* node = channels->list;
    while (node->next != NULL) {
        if (strcmp(node->remote_ip, ip)) {
            strcpy(buf, node->data);
            free(node->data);
            node->prev->next = node->next;
            free(node);
        }
        node = node->next;
    }
    pthread_mutex_unlock(&channels->lock);
    
}