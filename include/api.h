#ifndef _API_H_
#define _API_H_


#include "tju_tcp.h"

tju_tcp_t* tcp_socket();
int tcp_bind(tju_tcp_t* sock, tju_sock_addr bind_addr);
int tcp_listen(tju_tcp_t* sock);
int tcp_accept(tju_tcp_t* listen_sock, tju_tcp_t* conn_sock);
int tcp_connect(tju_tcp_t* sock, tju_sock_addr target_addr);
int tcp_send(tju_tcp_t* sock, const void *buffer, int len);
int tcp_recv(tju_tcp_t* sock, void *buffer, int len);
int tcp_close (tju_tcp_t* sock);

#endif