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
    startSimulation();

    tju_tcp_t* my_server = tju_socket();
    // printf("my_tcp state %d\n", my_server->state);
    
    tju_sock_addr bind_addr;
    bind_addr.ip = inet_network("10.0.0.3");
    bind_addr.port = 1234;

    // bind server ip
    tju_bind(my_server, bind_addr);
    // listen server
    tju_listen(my_server);
    // init thread pool
    threadpool thpool = thpool_init(QUEUES);
    sleep(5);
    // listen client connect
    while (TRUE) {
        tju_tcp_t* client = tju_accept(my_server);
        thpool_add_work(thpool, echo, (void*)client);
    }

	thpool_wait(thpool);
	puts("Killing threadpool");
	thpool_destroy(thpool);

    return EXIT_SUCCESS;
}
