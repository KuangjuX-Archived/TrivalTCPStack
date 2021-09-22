#include "api.h"
#include "kernel.h"
#include <string.h>


int main(int argc, char **argv) {
    // 开启仿真环境 
    startSimulation();

    tju_tcp_t* my_socket = tcp_socket();
    
    tju_sock_addr target_addr;
    target_addr.ip = inet_network("10.0.0.3");
    target_addr.port = 1234;

    tcp_connect(my_socket, target_addr);
    for(int i = 10; i < 99; i++) {
        char* msg = (char*)malloc(7);
        sprintf(msg, "echo %d", i);
        printf(GRN "[发送消息] %s.\n" RESET, msg);
        tcp_send(my_socket, msg, 7);
        // sleep(1);
    }
    while(TRUE){}
    // tcp_close(my_socket);
    return EXIT_SUCCESS;
}
