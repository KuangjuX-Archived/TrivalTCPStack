#include "tcp_manager.h"
#include <stdlib.h>

tcp_manager_t* tcp_manager;
extern const int socket_size;

void tcp_manager_init() {
    tcp_manager = (tcp_manager_t*)malloc(sizeof(tcp_manager_t));
    for(int i = 0; i < MAX_SOCK_SIZE; i++) {
        tcp_manager->listen_queue[i] = (struct tju_tcp_t*)malloc(sizeof(tju_tcp_t));
        tcp_manager->listen_queue[i] = NULL;

        tcp_manager->connect_sock[i] = (struct tju_tcp_t*)malloc(sizeof(tju_tcp_t));
        tcp_manager->connect_sock[i] = NULL;
    }
}

tcp_manager_t* get_tcp_manager() {
    return tcp_manager;
}

