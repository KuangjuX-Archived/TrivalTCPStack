#include "api.h"
#include "kernel.h"
#include <string.h>


int main(int argc, char **argv) {
    // 开启仿真环境 
    startSimulation();

    tju_tcp_t* my_socket = tcp_socket();
    // printf("my_tcp state %d\n", my_socket->state);
    
    tju_sock_addr target_addr;
    target_addr.ip = inet_network("10.0.0.3");
    target_addr.port = 1234;

    tcp_connect(my_socket, target_addr);
    for(int i = 0; i < 10; i++) {
        char* msg = (char*)malloc(8);
       sprintf(msg, "echo msg");
       printf("[发送消息] %s.\n", msg);
        tcp_send(my_socket, msg, 8);
    }
    while(TRUE){}
    // tcp_close(my_socket);
    return EXIT_SUCCESS;
}
