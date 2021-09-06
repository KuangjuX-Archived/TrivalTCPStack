#ifndef _CONSTS_H_
#define _CONSTS_H_

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

// #define SYN (1<<3)
// #define ACK (1<<2)
// #define FIN (1<<1)

// TCP 拥塞控制状态
#define SLOW_START 0
#define CONGESTION_AVOIDANCE 1
#define FAST_RECOVERY 2

// TCP 接受窗口大小
#define TCP_RECVWN_SIZE 32 * MAX_DLEN // 比如最多放32个满载数据包
#define TCP_RECV_WINDOW_SIZE 128 * MAX_DLEN

// TCP 发送缓存窗口大小
#define TCP_SEND_WINDOW_SIZE 128 * MAX_DLEN

// TCP send/recv buffer size
#define TCP_SEND_BUFFER_SIZE 65535 //max for uint16
#define TCP_RECV_BUFFER_SIZE 65535 //max for uint16

// TCP改进窗口算法的阈值
#define IMPROVED_WINDOW_THRESHOLD 0.25

// the max size of thread pool
#define THREAD_POOL_SIZE 16

/*
=========================================================
========================timer常量=========================
=========================================================
*/

#define ALPHA (1/8)
#define BETA (1/4)

// clock granularities
#define CLOCK_G (0.1)

#define TCP_RTO_MAX (100)

#define ACK_SIGNAL (0)
#define TIMEOUT_SIGNAL (1)

/*
  =====================================================
  ===================Packet相关=========================
  ===================================================== 
*/

// 重传次数限制
#define RETRANSMIT_LIMIT 10


#define DEFAULT_HEADER_LEN 22
#define SYN_FLAG_MASK 0x8
#define ACK_FLAG_MASK 0x4
#define FIN_FLAG_MASK 0x2

#define HEADER_LEN 22

#endif