#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#include <netinet/in.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "global.h"
#include <pthread.h>
#include <sys/select.h>
#include <arpa/inet.h>
// #include "timer.h"

#define MSL 1

// 单位是byte
#define SIZE32 4
#define SIZE16 2
#define SIZE8  1

// 一些Flag
#define NO_FLAG 0
#define NO_WAIT 1
#define TIMEOUT 2
#define TRUE 1
#define FALSE 0

// 定义最大包长 防止IP层分片
#define MAX_DLEN 1375 	// 最大包内数据长度
#define MAX_LEN 1400 	// 最大包长度

// TCP socket 状态定义
#define CLOSED 0
#define LISTEN 1
#define SYN_SENT 2
#define SYN_RECV 3
#define ESTABLISHED 4
#define FIN_WAIT_1 5
#define FIN_WAIT_2 6
#define CLOSE_WAIT 7
#define CLOSING 8
#define LAST_ACK 9
#define TIME_WAIT 10

// 标志位定义
#define FIN (1<<0)
#define SYN (1<<1)
#define RST (1<<2)
#define PSH (1<<3)
#define ACK (1<<4)
#define URG (1<<5)

// TCP 拥塞控制状态
#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define FAST_RECOVERY 2

// TCP 接受窗口大小
#define TCP_RECVWN_SIZE 32*MAX_DLEN // 比如最多放32个满载数据包

// TCP 发送缓存窗口大小
#define TCP_SEND_SIZE 128
#define MAX_SENDING_ITEM_NUM 1024 //最大file数

// TCP改进窗口算法的阈值
#define IMPROVED_WINDOW_THRESHOLD 0.25

// the max size of thread pool
#define THREAD_POOL_SIZE 16

// TCP 发送窗口
// 注释的内容如果想用就可以用 不想用就删掉 仅仅提供思路和灵感
typedef struct {
	tju_packet_t send_windows[TCP_SEND_SIZE];
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
	char received[TCP_RECVWN_SIZE];

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

// TJU_TCP 结构体 保存TJU_TCP用到的各种数据
typedef struct tju_tcp_t {
	int state; // TCP的状态

	tju_sock_addr bind_addr; // 存放bind和listen时该socket绑定的IP和端口
	tju_sock_addr established_local_addr; // 存放建立连接后 本机的 IP和端口
	tju_sock_addr established_remote_addr; // 存放建立连接后 连接对方的 IP和端口

	pthread_mutex_t send_lock; // 发送数据锁
	char* sending_buf; // 发送数据缓存区
	int sending_len; // 发送数据缓存长度
	int sending_item_flag[MAX_SENDING_ITEM_NUM];

	pthread_mutex_t recv_lock; // 接收数据锁
	char* received_buf; // 接收数据缓存区
	int received_len; // 接收数据缓存长度

	pthread_cond_t wait_cond; // 可以被用来唤醒recv函数调用时等待的线程

	window_t window; // 发送和接受窗口

	int saved_syn;

	// 计时器逻辑
	struct rtt_timer_t* rtt_timer;

} tju_tcp_t;

extern char* client_ip;
extern char* server_ip;

#endif