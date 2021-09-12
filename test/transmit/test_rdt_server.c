#include "tju_tcp.h"
#include "api.h"
#include <string.h>
#include <signal.h>

void sleep_no_wake(int sec){  
    do{        
        printf("Interrupted\n");
        sec =sleep(sec);
    }while(sec > 0);             
}

int main(int argc, char **argv) {
    // 开启仿真环境 
    startSimulation();
    
    tju_tcp_t* my_server = tcp_socket();
    
    tju_sock_addr bind_addr;
    bind_addr.ip = inet_network("10.0.0.1");
    bind_addr.port = 1234;
    
    tcp_bind(my_server, bind_addr);

    tcp_listen(my_server);
    tju_tcp_t* new_conn = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));

    tcp_accept(my_server, new_conn);


    sleep_no_wake(8);

    for (int i=0; i<50; i++){
        char buf[16];
        tcp_recv(new_conn, (void*)buf, 16);
        printf("[RDT TEST] server recv %s", buf);
        fflush(stdout);
    }
    sleep_no_wake(100);
    


    return EXIT_SUCCESS;
}
