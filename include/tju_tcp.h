#ifndef _TJU_TCP_H_
#define _TJU_TCP_H_

#include "global.h"
#include "tju_packet.h"
#include "kernel.h"


/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket();

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr);

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
*/
int tju_listen(tju_tcp_t* sock);

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
// tju_tcp_t* tju_accept(tju_tcp_t* sock);
int tcp_accept(tju_tcp_t* listen_sock, tju_tcp_t* conn_sock);


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
// int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr);
int tcp_connect(tju_tcp_t* sock, tju_sock_addr target_addr);


int tju_send (tju_tcp_t* sock, const void *buffer, int len);
int tju_recv (tju_tcp_t* sock, void *buffer, int len);

/*
关闭一个TCP连接
这里涉及到四次挥手
*/
int tcp_close (tju_tcp_t* sock);

// 服务端状态处理
int tcp_rcv_state_server(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_addr);

// 客户端状态处理
int tcp_rcv_state_client(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_sock);

// 关闭处理
int tcp_state_close(tju_tcp_t* local_sock, char* recv_pkt);

// 传输控制位packet
void tcp_send_fin(tju_tcp_t* sock);
void tcp_send_ack(tju_tcp_t* sock);

void tcp_update_expected_seq(tju_tcp_t* tcp, tju_packet_t* pkt);


int handle_improved_window();

void load_data_to_sending_window(tju_tcp_t *sock, const void *pVoid, int len);
void calculate_sending_buffer_depend_on_rwnd(tju_tcp_t* sock);
void handle_delay_ack(tju_tcp_t* sock, char* pkt);
#endif

