#ifndef _TRIVIAL_EPOLL_H_
#define _TRIVIAL_EPOLL_H_

#define MAX_EVENT_NUMS 1024
#define MAX_EPOLL_SIZE 1024

typedef void (*udp_recv_callback)(int, struct sockaddr_in*, const char*, unsigned int);

typedef struct epoll_server_t {
    int epoll_fd;
    int listen;
    int stop_flag;

    udp_recv_callback callback_handler;
} epoll_server_t;

#endif