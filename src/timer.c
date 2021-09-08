#include "timer.h"
#include "kernel.h"
#include "chan.h"
#include "utils.h"

int tcp_close (tju_tcp_t* sock);

// TODO: sock作为参数传入是否可以当作全局变量， 
// 是否需要传入index从哈希表中获取
// 检查是否超时
void* tcp_check_timeout(void* arg) {
    // 这里我们异步监测是否超时，倘若超时则调用回调函数
    // 否则监测到中断信号返回
    tju_tcp_t* sock = (tju_tcp_t*)arg;
    float timeout = sock->rtt_timer->timeout;
    time_t cur_time = time(NULL);
    // 初始化信号量
    while(difftime(time(NULL), cur_time) < timeout) {
        if (sock->interrupt_signal == 1) {
            printf("[Timer] 接收到中断信号.\n");
            // 更新RTT的值
            tcp_ack_update_rtt(sock, time(NULL) - cur_time, 1000);
            return NULL;
        }else {
            // 休息一会，防止不断轮询导致CPU负载过重
            sleep(1);
        }
    }
    // 此时超时，调用回调函数
    sock->rtt_timer->callback(sock);
}

void tcp_init_timer(
    tju_tcp_t* sock, 
    void (*retransmit_handler)(unsigned long)
) {
    tcp_init_rtt(sock);
    sock->rtt_timer->callback = retransmit_handler;
    sock->rtt_timer->started = 0;
    pthread_mutex_init(&sock->rtt_timer->timer_lock, NULL);
}

void tcp_init_rtt(struct tju_tcp_t* sock) {
    sock->rtt_timer = (rtt_timer_t*)malloc(sizeof(rtt_timer_t));
    sock->rtt_timer->estimated_rtt = 1000;
    sock->rtt_timer->dev_rtt = 1000;
    sock->rtt_timer->timeout = 1000;
    sock->rtt_timer->timer_thread = 0;
}

void tcp_set_estimator(tju_tcp_t* sock, float mrtt_us) {
    sock->rtt_timer->estimated_rtt = (1 - ALPHA) *  sock->rtt_timer->estimated_rtt + ALPHA * mrtt_us;
    sock->rtt_timer->dev_rtt = (1 - BETA) * sock->rtt_timer->dev_rtt + BETA * abs(sock->rtt_timer->estimated_rtt - mrtt_us);
}

void tcp_bound_rto(tju_tcp_t* sock) {
    if(sock->rtt_timer->timeout > TCP_RTO_MAX) {
        sock->rtt_timer->timeout = TCP_RTO_MAX;
    }
}

void tcp_set_rto(tju_tcp_t* sock) {
    sock->rtt_timer->timeout = sock->rtt_timer->estimated_rtt = max(CLOCK_G, 4 * sock->rtt_timer->dev_rtt);
    tcp_bound_rto(sock);
}

int tcp_ack_update_rtt(tju_tcp_t* sock, float seq_rtt_us, float sack_rtt_us) {

    /* Prefer RTT measured from ACK's timing to TS-ECR. This is because
	 * broken middle-boxes or peers may corrupt TS-ECR fields. But
	 * Karn's algorithm forbids taking RTT if some retransmitted data
	 * is acked (RFC6298).
	 */
	if (seq_rtt_us < 0)
		seq_rtt_us = sack_rtt_us;

    tcp_set_estimator(sock, seq_rtt_us);
    tcp_set_rto(sock);
    return 0;
}

// 当计时器超时时的回调函数
void tcp_write_timer_handler(tju_tcp_t* sock) {
    printf("[Timer] 超时了！\n");
    // 这里需要针对socket的状态进行不同的操作
    switch(sock->state) {
        case SYN_SENT:
            tcp_send_syn(sock);
        case SYN_RECV:  
            tcp_send_syn_ack(sock);
        case ESTABLISHED: 
            // 超时重传，这里或许需要判断一下重传的次数，若重传次数过多应该关闭连接
            if(sock->timeout_counts > RETRANSMIT_LIMIT) {
                // 重传次数超限，关闭连接
                printf("[超时处理] 重传次数超限，关闭连接.\n");
                tcp_outlimit_retransmit(sock);
            }else {
                // 重传分组
                handle_loss_ack(sock);
                sock->timeout_counts += 1;
                tcp_retransmit_timer(sock);
            }
        default:
            printf("[超时处理] 未解决的状态.\n");
    }
}


// 开始计时，创建新线程
void tcp_start_timer(tju_tcp_t* sock) {
    // 当计时器仍在运行时，我们需要首先关闭计时器，再重新开启计时器
    if(sock->rtt_timer->started == 1) {
        printf("[Timer] 计时器已经开启，不能重复开启.\n");
        // return;
        tcp_stop_timer(sock);
    }
    pthread_mutex_lock(&sock->rtt_timer->timer_lock);
    sock->rtt_timer->started = 1;
    pthread_mutex_unlock(&sock->rtt_timer->timer_lock);
    pthread_mutex_lock(&sock->signal_lock);
    sock->interrupt_signal = 0;
    pthread_mutex_unlock(&sock->signal_lock);
    printf("[Timer] 开始计时.\n");
    // 这里应当生成新线程，异步监视是否超时
    pthread_create(&sock->rtt_timer->timer_thread, NULL, tcp_check_timeout, (void*)sock);
}

// 停止计时并返回ack所花费时间
void tcp_stop_timer(tju_tcp_t* sock) {
    // 当计时器i已经停止时，我们只需要退出即可
    if(sock->rtt_timer->started == 0) {
        printf("[Timer] 计时器已经中断，不能重复中断.\n");
        return;
    }
    pthread_mutex_lock(&sock->rtt_timer->timer_lock);
    sock->rtt_timer->started = 0;
    pthread_mutex_unlock(&sock->rtt_timer->timer_lock);
    pthread_mutex_lock(&sock->signal_lock);
    sock->interrupt_signal = 1;
    pthread_mutex_unlock(&sock->signal_lock);
    // 等待进程结束进行同步
    pthread_join(sock->rtt_timer->timer_thread, NULL);
    printf("[Timer] 结束计时.\n");
}


// 超时重传函数处理
void tcp_retransmit_timer(tju_tcp_t* sock) {
    int base = sock->window.wnd_send->base % TCP_SEND_WINDOW_SIZE;
    int next_seq = sock->window.wnd_send->nextseq % TCP_SEND_WINDOW_SIZE;
    int len = next_seq - base;
    char* buf = (char*)malloc(len);
    memcpy(buf, sock->window.wnd_send->send_windows + base, len);

    uint16_t plen = DEFAULT_HEADER_LEN + len;
    uint32_t seq = sock->window.wnd_send->nextseq;
    char* msg;
    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, buf, len);
    
    // 打开计时器
    tcp_start_timer(sock);
    sendToLayer3(msg, plen);
}

void handle_loss_ack(tju_tcp_t* sock) {
    int timeout_counts=sock->timeout_counts%4;
    if(sock->con_status==SLOW_START){
        sock->ssthresh=(sock->cwnd+1)/2;
        sock->cwnd=1*SMSS;
        if(sock->cwnd>sock->ssthresh){
            sock->con_status=CONGESTION_AVOIDANCE;
        }
    }else if(sock->con_status==CONGESTION_AVOIDANCE&&timeout_counts==3){
        sock->ssthresh=(sock->cwnd+1)/2;
        sock->cwnd=sock->ssthresh+3*SMSS;
        sock->con_status=FAST_RECOVERY;
    }else if(sock->con_status==FAST_RECOVERY){
        sock->ssthresh=(sock->cwnd+1)/2;
        sock->cwnd=1;
        sock->con_status=SLOW_START;
    }else{
        printf("不存在相应状态\n");
    }
}

// 重传次数太多，此时应当主动关闭连接
void tcp_outlimit_retransmit(tju_tcp_t* sock) {
    tcp_close(sock);
}