#include "tju_tcp.h"
#include <string.h>


int main(int argc, char **argv) {
    // 开启仿真环境 
    startSimulation();

    tju_tcp_t* my_socket = tju_socket();
    // printf("my_tcp state %d\n", my_socket->state);
    
    tju_sock_addr target_addr;
    target_addr.ip = inet_network("10.0.0.3");
    target_addr.port = 1234;

    // tju_connect(my_socket, target_addr);
    tcp_connect(my_socket, target_addr);

    sleep(3);

    tju_send(my_socket, "hello world", 12);
    tju_send(my_socket, "hello tju", 10);

    char buf[2021];
    tju_recv(my_socket, (void*)buf, 12);
    printf("client recv %s\n", buf);

    tju_recv(my_socket, (void*)buf, 10);
    printf("client recv %s\n", buf);

    return EXIT_SUCCESS;
}
