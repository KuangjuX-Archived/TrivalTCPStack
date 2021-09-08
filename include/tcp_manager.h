#ifndef _TCP_MANAGER_H_
#define _TCP_MANAGER_H_

#include "tju_tcp.h"
#include "consts.h"


typedef struct tcp_manager_t {
    // 监听队列，应当放在全局变量中被管理者管理
    tju_tcp_t* listen_queue[MAX_SOCK_SIZE];
    // 建立连接队列
    tju_tcp_t* established_queue[MAX_SOCK_SIZE];
    // 对于客户端来说，这里存储连接的socket
    tju_tcp_t* connect_sock[MAX_SOCK_SIZE];
    // 判断是服务端还是客户端(其实不应该有，因为每台机器既可以当客户端又可以当服务端)
    int is_server;

} tcp_manager_t;

void tcp_manager_init();
tcp_manager_t* get_tcp_manager();
tju_tcp_t* get_listen_sock(int index);
tju_tcp_t* get_established_sock(int index);
tju_tcp_t* get_connect_sock(int index);

#endif