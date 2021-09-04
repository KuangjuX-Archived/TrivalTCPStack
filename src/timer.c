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
    time_t out_time = cur_time + timeout;
    // 初始化信号量
    char* signal = (char*)malloc(10);
    while(time(NULL) < out_time) {
        // 检查信号量
        // 注：select方法为非阻塞读取channel，当没有读取到对应的数据，将会立刻向下执行
        switch (chan_select(sock->rtt_timer->chan, NULL, &signal, NULL, 0, NULL)) {
            case 0:
                if(strcmp(signal, "interrupt")) {
                    // 更新RTT的值
                    tcp_ack_update_rtt(sock, time(NULL) - cur_time, 1);
                    // 销毁channel
                    chan_dispose(sock->rtt_timer->chan);
                    sock->rtt_timer->chan = NULL;
                    return NULL;
                }
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
}

void tcp_init_rtt(struct tju_tcp_t* sock) {
    sock->rtt_timer = (rtt_timer_t*)malloc(sizeof(rtt_timer_t));
    sock->rtt_timer->estimated_rtt = 1;
    sock->rtt_timer->dev_rtt = 1;
    sock->rtt_timer->timeout = 100;
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
    printf("timeout.\n");
    chan_dispose(sock->rtt_timer->chan);
    sock->rtt_timer->chan = NULL;
    // 这里需要针对socket的状态进行不同的操作
    switch(sock->state) {
        // 这里暂时先不考虑三次握手超时的情况
        case SYN_SENT:
            printf("transmit RST.\n");
        case ESTABLISHED: 
            // 超时重传，这里或许需要判断一下重传的次数，若重传次数过多应该关闭连接
            if(sock->timeout_counts > RETRANSMIT_LIMIT) {
                // 重传次数超限，关闭连接
                printf("重传次数超限，关闭连接.\n");
                tcp_outlimit_retransmit(sock);
            }else {
                // 重传分组
                sock->timeout_counts += 1;
                tcp_retransmit_timer(sock);
            }
        default:
            printf("Unresolved status.\n");
    }
}

// 开始计时，创建新线程
void tcp_start_timer(tju_tcp_t* sock) {
    // 建立无缓冲区的channel
    sock->rtt_timer->chan = chan_init(0);
    // 这里应当至今生成新线程，异步监视是否超时
    pthread_create(&sock->rtt_timer->timer_thread, NULL, tcp_check_timeout, (void*)sock);
}

// 停止计时并返回ack所花费时间
void tcp_stop_timer(tju_tcp_t* sock) {
    char* data = "interrupt";
    if(sock->rtt_timer->chan != NULL) {
        // 发送中断信号
        chan_send(sock->rtt_timer->chan, (void*)data);
    }
    // 等待进程结束进行同步
    pthread_join(sock->rtt_timer->timer_thread, NULL);
}


// 超时重传函数处理
void tcp_retransmit_timer(tju_tcp_t* sock) {
    int base = sock->window.wnd_send->base % TCP_SEND_WINDOW_SIZE;
    int next_seq = sock->window.wnd_send->nextseq % TCP_SEND_WINDOW_SIZE;
    int len = next_seq - base;
    char* buf = (char*)malloc(next_seq - base);
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

// 慢启动
void tcp_entry_loss(tju_tcp_t* sock) {

}

// 重传次数太多，此时应当主动关闭连接
void tcp_outlimit_retransmit(tju_tcp_t* sock) {
    tcp_close(sock);
}