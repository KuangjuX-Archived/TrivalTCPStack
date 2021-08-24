#include "tju_tcp.h"
#include "thpool.h"
#include <string.h>

#define QUEUES 16
#define SIZE 8192


void echo(void* client) {
    tju_tcp_t* socket = (tju_tcp_t*) client;
    printf("Start receive client message.\n");
    while (TRUE) {
        char buf[512];
        tju_recv(socket, (void*)buf, 512);
        printf("%s\n", buf);
        tju_send(socket, (void*)buf, 512);
    }
}

int main(int argc, char **argv) {
    // 开启仿真环境 
    printf("Start simulation.\n");
    startSimulation();

    printf("create server socket.\n");
    tju_tcp_t* my_server = tju_socket();
    
    tju_sock_addr bind_addr;
    bind_addr.ip = inet_network("10.0.0.3");
    bind_addr.port = 1234;

    // bind server ip
    printf("bind server address.\n");
    tju_bind(my_server, bind_addr);

    // listen server
    printf("listen server address.\n");
    tju_listen(my_server);

    printf("Try to accept client.\n");
    tju_tcp_t* conn_sock;
    conn_sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    int status = tcp_accept(my_server, conn_sock);
    if(status < 0) {
        printf("Fail to accept.\n");
    }

    char buf[512];
    tju_recv(conn_sock, (void*)buf, 512);
    printf("%s\n", buf);
    tju_send(conn_sock, (void*)buf, 512);

    tju_recv(conn_sock, (void*)buf, 512);
    printf("%s\n", buf);
    tju_send(conn_sock, (void*)buf, 512);
    while(TRUE){}

    return EXIT_SUCCESS;
}
