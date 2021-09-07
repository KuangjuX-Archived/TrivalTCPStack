#ifndef _TJU_TCP_H_
#define _TJU_TCP_H_


#include "tju_packet.h"
#include "consts.h"
struct sockqueue;

// TCP 发送窗口
// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
typedef struct {
	char* send_windows;
	uint16_t window_size;
  	uint32_t base;
    uint32_t nextseq;
//   uint32_t estmated_rtt;
    int ack_cnt;
    pthread_mutex_t ack_cnt_lock;
//   struct timeval send_time;
//   struct timeval timeout;
    uint16_t rwnd; 
//   int congestion_status;
//   uint16_t cwnd; 
//   uint16_t ssthresh; 
} sender_window_t;

// TCP 接受窗口
// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
// 发现窗口用字节数描述太难实现了，暂时使用packet来代替
typedef struct {
	char* receiver_window;

    //  received_packet_t* head;
    char buf[TCP_RECVWN_SIZE];
    uint8_t marked[TCP_RECVWN_SIZE];
    uint32_t expect_seq;
} receiver_window_t;

// TCP 窗口 每个建立了连接的TCP都包括发送和接受两个窗口
typedef struct {
	sender_window_t* wnd_send;
  	receiver_window_t* wnd_recv;
} window_t;

typedef struct {
	uint32_t ip;
	uint16_t port;
} tju_sock_addr;


struct rtt_timer_t; 

// typedef struct received_buf {
// 	int capacity;
// 	int size;
// 	char* buf;
// } received_buf;

// TJU_TCP 结构体 保存TJU_TCP用到的各种数据
typedef struct tju_tcp_t {
	int state; // TCP的状态

	tju_sock_addr bind_addr; // 存放bind和listen时该socket绑定的IP和端口
	tju_sock_addr established_local_addr; // 存放建立连接后 本机的 IP和端口
	tju_sock_addr established_remote_addr; // 存放建立连接后 连接对方的 IP和端口

	pthread_mutex_t send_lock; // 发送数据锁
	pthread_t send_thread; // 发送线程
	char* sending_buf; // 发送数据缓存区
	int sending_len; // 发送数据缓存长度
	int sending_capacity; // 发送缓冲区容量

	pthread_mutex_t recv_lock; // 接收数据锁
	char* received_buf; // 接收数据缓存区
	int received_len; // 接收数据缓存长度
	int received_capacity; // 接受缓冲区容量

	pthread_cond_t wait_cond; // 可以被用来唤醒recv函数调用时等待的线程

	window_t window; // 发送和接受窗口

	// 保存的半连接队列id
	int saved_syn;

	pthread_mutex_t signal_lock;
	int interrupt_signal;

	// 计时器逻辑
	struct rtt_timer_t* rtt_timer;

	// 超时次数
	uint8_t timeout_counts;

	// 这些域仅在server端被使用
	// 半连接队列
	struct sock_queue* syn_queue;
	// 全连接队列
	struct sock_queue* accept_queue;
	// 已建立连接队列
	struct tju_tcp_t* established_queue[MAX_SOCK_SIZE];

    // 拥塞控制相关
    int con_status;
    int cwnd;
    int ssthresh;

} tju_tcp_t;


// 服务端状态处理
int tcp_rcv_state_server(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_addr);

// 客户端状态处理
int tcp_rcv_state_client(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_sock);

// 关闭处理
int tcp_state_close(tju_tcp_t* local_sock, char* recv_pkt);

// 传输控制位packet
void tcp_send_fin(tju_tcp_t* sock);
void tcp_send_syn(tju_tcp_t* sock);
void tcp_send_syn_ack(tju_tcp_t* sock);
void tcp_send_ack(tju_tcp_t* sock, int len);

void tcp_update_expected_seq(tju_tcp_t* sock, char* pkt);

/*
================================================
===================检验和========================
================================================
*/

// 设置checksum
void set_checksum(tju_packet_t* pkt);

int tcp_check(tju_packet_t* pkt);

int tcp_check_seq(tju_packet_t* pkt, tju_tcp_t* sock);

// 计算checksum
static unsigned short tcp_compute_checksum(tju_packet_t* pkt);

void set_checksum(tju_packet_t* pkt);

/*
===================================================
=======================流量控制======================
===================================================
*/



int handle_improved_window();

void load_data_to_sending_window(tju_tcp_t *sock, const void *pVoid, int len);
int calculate_sending_buffer_depend_on_rwnd(tju_tcp_t* sock);
void handle_delay_ack(tju_tcp_t* sock, char* pkt);
int improve_send_wnd(tju_tcp_t* sock);
void keep_alive(tju_tcp_t *sock);
#endif
