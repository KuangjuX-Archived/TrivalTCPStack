#include "tju_tcp.h"
#include "api.h"
#include <string.h>


int main(int argc, char **argv) {
    // 开启仿真环境 
    startSimulation();

    tju_tcp_t* my_socket = tcp_socket();
    
    tju_sock_addr target_addr;
    target_addr.ip = inet_network("10.0.0.1");
    target_addr.port = 1234;

    tcp_connect(my_socket, target_addr);

    

    return EXIT_SUCCESS;
}
