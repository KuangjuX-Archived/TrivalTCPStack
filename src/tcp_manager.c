#include "tcp_manager.h"
#include <stdlib.h>

tcp_manager_t* tcp_manager;

// 初始化 tcp_manager
void tcp_manager_init() {
    tcp_manager = (tcp_manager_t*)malloc(sizeof(tcp_manager_t));
    for(int i = 0; i < MAX_SOCK_SIZE; i++) {
        tcp_manager->listen_queue[i] = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
        tcp_manager->listen_queue[i] = NULL;

        tcp_manager->established_queue[i] = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
        tcp_manager->established_queue[i] = NULL;

        tcp_manager->connect_sock[i] = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
        tcp_manager->connect_sock[i] = NULL;
    }
}

// 获取 tcp_manager
tcp_manager_t* get_tcp_manager() {
    return tcp_manager;
}

tju_tcp_t* get_listen_sock(int index) {
    return tcp_manager->listen_queue[index];
}

tju_tcp_t* get_established_sock(int index) {
    return tcp_manager->established_queue[index];
}

tju_tcp_t* get_connect_sock(int index) {
    return tcp_manager->connect_sock[index];
}

