#ifndef _TIMER_H_
#define _TIMER_H_

#include "kernel.h"
#include "utils.h"
#include "global.h"
#include "chan.h"

#define ALPHA (1/8)
#define BETA (1/4)

// clock granularities
#define CLOCK_G (0.1)

#define TCP_RTO_MAX (100)

#define ACK_SIGNAL (0)
#define TIMEOUT_SIGNAL (1)

typedef struct rtt_timer_t {
    float estimated_rtt;
    float dev_rtt;
    float timeout;
    void (*callback)(tju_tcp_t* sock);
    chan_t* chan;
    pthread_t timer_thread;
} rtt_timer_t;

void tcp_init_timer(tju_tcp_t* sock, void (*retransmit_handler)(unsigned long));
void tcp_init_rtt(tju_tcp_t* sock);
void tcp_set_estimator(tju_tcp_t* sock, float mrtt_us);
void tcp_bound_rto(tju_tcp_t* sock);
void tcp_set_rto(tju_tcp_t* sock);
int tcp_ack_update_rtt(tju_tcp_t* sock, float seq_rtt_us, float sack_rtt_us);

void tcp_start_timer(tju_tcp_t* sock);
void tcp_stop_timer(tju_tcp_t* sock);

void tcp_write_timer_handler(tju_tcp_t* sock);

#endif