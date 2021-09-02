#include <sys/epoll.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "trivial_epoll.h"

struct epoll_event* events;

void event_init() {
    events = (struct epoll_event*)malloc(sizeof(struct epoll_event) * MAX_EVENT_NUMS);
}

int create_udp_listen(short port) {
    int sockFd, ret;
    struct sockaddr_in seraddr;

    bzero(&seraddr, sizeof(struct sockaddr_in));
    seraddr.sin_family = AF_INET;
    seraddr.sin_addr.s_addr = INADDR_ANY;
    seraddr.sin_port = htons(port);

    sockFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFd < 0) {
        perror("socket error");
        return -1;
    }

    if (setnonblocking(sockFd) < 0) {
        perror("setnonblock error");
    }

    ret = bind(sockFd, (struct sockaddr*)&seraddr, sizeof(struct sockaddr));
    if (ret < 0) {
        perror("bind");
        return -1;
    }

    return sockFd;
}

int epoll_event_loop() {
   
}