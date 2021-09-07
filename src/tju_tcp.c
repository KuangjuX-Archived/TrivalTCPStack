#include <kernel.h>
#include <pthread.h>
#include "tju_tcp.h"
#include "sockqueue.h"
#include "utils.h"

void sendToLayer3(char* packet_buf, int packet_len);
int cal_hash(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port);

void tcp_start_timer(tju_tcp_t* sock);
void tcp_stop_timer(tju_tcp_t* sock);

const int socket_size = sizeof(tju_tcp_t);
/*
====================================================
===========以下为辅助函数的实现，仅在该模块被调用=========
====================================================
*/

// extern sock_queue* syns_socks;
// extern sock_queue* accept_socks;
extern tju_tcp_t* connect_sock;


/*
======================================================
=======================连接管理========================
======================================================
*/

// 这里传输的应该是建立连接的socket
int tcp_rcv_state_server(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_addr) {
    uint8_t flags = get_flags(pkt);
    sock->window.wnd_send->rwnd= get_advertised_window(pkt);
    switch (sock->state) {
        case CLOSED:
            // 关闭状态，不能接受任何消息
            printf("Closed state can't receive messages.\n");
            return -1;
        case LISTEN:
            // 判断packet flags 是否为 SYN_SENT 并判断ack的值
            if (flags == SYN) {
                printf("[Debug] Server receive SYN request.\n");
                // 第二次握手，服务端修改自己的状态，
                // 并且发送 SYN_RECV 标志的pakcet，
                // 加入到半连接哈希表中（暂时不考虑重置状态）                
                // 这里应当构建客户端socket, 从而可以加入到syns_socks中
                tju_tcp_t* conn_sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
                conn_sock->bind_addr.ip = conn_addr->ip;
                conn_sock->bind_addr.port = conn_addr->port;

                // 第一次收到消息，初始化接收窗口
                tcp_update_expected_seq(sock, pkt);

                // 修改服务端socket的状态
                sock->state = SYN_SENT;
                // 加入到半连接哈希表中
                int index = sockqueue_push(sock->syn_queue, conn_sock);
                if (index < 0) {
                    printf("Fail to push syns_socks.\n");
                    return -1;
                }
                // 将syn_id存储进服务器socket中
                sock->saved_syn = index;
                // 创建带有状态的packet
                char* send_pkt;
                int len = build_state_pkt(pkt, &send_pkt, (SYN | ACK));
                // 发送packet到客户端
                tcp_start_timer(sock);
                sendToLayer3(send_pkt, len);
                return 0;
            } else {
                // 当flags不是SYN_SENT，暂时忽略
                printf("TrivialTCP should receive SYN_SENT pakcet.\n");
                return -1;
            }
        case SYN_SENT:
            if (flags == ACK) {
                tcp_stop_timer(sock);
                // 第三次握手，服务端发送ACK报文， 服务端将自己的socket变为ESTABLISHED，
                // 从syns_socks取出对应的socket并加入到accept_socks中
                // 处理完一个socket的三次握手过程，将socket的状态改为LISTEN
                sock->state = LISTEN;

                // 更新expected_seq
                tcp_update_expected_seq(sock, pkt);
                
                // 获取半连接队列的id
                int index = sock->saved_syn;
                // 获取半连接socket
                tju_tcp_t* conn_sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
                sockqueue_remove(sock->syn_queue, conn_sock, index);
                // 将半连接socket放到全连接socket中
                conn_sock->state = ESTABLISHED;
                sockqueue_push(sock->accept_queue, conn_sock);
                return 0;
            } else {
                // 当flags不是ESTABLSHED时，暂时忽略
                printf("TrivialTCP should receive ESTABLISHED packet.\n");
                return -1;
            }
        default:         
            printf("[三次握手] 服务端： 未解决的的标志位 %s\n", flags);
            return -1;
    }
}

int tcp_rcv_state_client(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_sock) {
    uint8_t flags = get_flags(pkt);
    sock->window.wnd_send->rwnd= get_advertised_window(pkt);
    switch(flags) {
        case (SYN | ACK):
            if(sock->state == SYN_SENT) {
                printf("[Debug] Client receive SYN response.\n");
                tcp_stop_timer(sock);
                // client首次收到ACK， 更新窗口expected_seq
                tcp_update_expected_seq(sock, pkt);
                // 随便编的seq
                int seq = get_seq(pkt) + 1;
                int ack = get_seq(pkt) + 1;
                // 构建packet发送给服务端
                uint16_t adv_wnd=TCP_SEND_BUFFER_SIZE-sock->received_len;
                char* send_pkt = create_packet_buf(
                    sock->established_local_addr.port,
                    conn_sock->port,
                    seq,
                    ack,
                    HEADER_LEN,
                    HEADER_LEN,
                    ACK,
                    adv_wnd,
                    0,
                    NULL,
                    0
                );
                sendToLayer3(send_pkt, HEADER_LEN);
                // 将socket的状态变为ESTABLISHED
                sock->state = ESTABLISHED;
                return 0;
            }else {
                printf("Error status.\n");
                return -1;
            }
        default:
            printf("[三次握手] 客户端： 未解决的的标志位 %s\n", flags);
            return -1;
    }
}

// 事实上除了通过判断recv_pkt的flags时也需要检查ack，这里暂时没有实现
// 此时传输的local_sock 为 ESTABLISHED sock
int tcp_state_close(tju_tcp_t* local_sock, char* recv_pkt) {
    uint8_t flags = get_flags(recv_pkt);
    switch(local_sock->state) {
        case ESTABLISHED:
            // 服务端接受客户端发起的close请求
            if(flags == (FIN | ACK)) {
                // 将本地socket状态修改为CLOS_WAIT
                local_sock->state = CLOSE_WAIT;
                // 向对方发送ACK
                tcp_send_ack(local_sock, 0);
                
                // 等待一段时间，继续向远程发送pakcet
                sleep(MSL * 2);
                local_sock->state = LAST_ACK;
                tcp_send_fin(local_sock);
                return 0;
            }else {
                printf("ESTABLISHED flags: %d.\n",flags);
                return -1;
            }

        case FIN_WAIT_1:
            if(flags == ACK) {
                // 这里将状态修改为FIN_WAIT_2,等待对方继续发送分组
                local_sock->state = FIN_WAIT_2;
                return 0;
            }else {
                return -1;
            }


        case FIN_WAIT_2:
            if(flags == (FIN | ACK)) {
                tcp_send_ack(local_sock, 0);
                local_sock->state = TIME_WAIT;
                // 等待2 * MSL 时间，释放socket资源
                sleep(2 * MSL);
                free(local_sock);
                // connect_sock = NULL;
                printf("CLOSED.\n");
                return 0;
            }else {
                return -1;
            }
        
        // case TIME_WAIT:
        //     if(flags == (FIN | ACK)) {
        //         tcp_send_ack(local_sock);
        //         // 这里应该等待两个2个MSL
        //         // 释放socket资源
        //         sleep(2 * MSL);
        //         free(local_sock);
        //         connect_sock = NULL;
        //         printf("CLOSED.\n");
        //         return 0;
        //     }else {
        //         return -1;
        //     }
        
        case LAST_ACK:
            // 关闭
            if(flags == ACK) {
                printf("CLOSED.\n");
                local_sock->state = CLOSED;
                // 释放socket资源
                free(local_sock);
                return 0;
            }else {
                return -1;
            }
        default: 
            printf("Unresolved status.\n");
            return -1;
    }
}

void tcp_send_fin(tju_tcp_t* sock) {
    char* send_pkt;
    // 瞎编seq和ack
    int seq = sock->window.wnd_recv->expect_seq;
    int ack = seq;
    int flags = ACK | FIN;
    send_pkt = create_packet_buf(
        sock->established_local_addr.port,
        sock->established_remote_addr.port,
        seq,
        ack,
        HEADER_LEN,
        HEADER_LEN,
        flags,
        0,
        0,
        NULL,
        0
    );
    sendToLayer3(send_pkt, HEADER_LEN);
}


void tcp_send_syn(tju_tcp_t* sock) {
    char* send_pkt;
    int seq = sock->window.wnd_recv->expect_seq;
    int ack = seq;
    int flags = SYN;
    send_pkt = create_packet_buf(
        sock->established_local_addr.port,
        sock->established_remote_addr.port,
        seq,
        ack,
        HEADER_LEN,
        HEADER_LEN,
        flags,
        0,
        0,
        NULL,
        0
    );
    sendToLayer3(send_pkt, HEADER_LEN);
}

void tcp_send_syn_ack(tju_tcp_t* sock) {
    char* send_pkt;
    int seq = sock->window.wnd_recv->expect_seq;
    int ack = seq;
    int flags = SYN | ACK;
    send_pkt = create_packet_buf(
        sock->established_local_addr.port,
        sock->established_remote_addr.port,
        seq,
        ack,
        HEADER_LEN,
        HEADER_LEN,
        flags,
        0,
        0,
        NULL,
        0
    );
    sendToLayer3(send_pkt, HEADER_LEN);
}

void tcp_send_ack(tju_tcp_t* sock, int len) {
    // 瞎编seq和ack
    uint32_t seq = sock->window.wnd_recv->expect_seq + len;
    uint32_t ack = seq;
    int rwnd=TCP_RECV_BUFFER_SIZE-sock->received_len;
    char* send_pkt = create_packet_buf(
        sock->established_local_addr.port,
        sock->established_remote_addr.port,
        seq,
        ack,
        HEADER_LEN,
        HEADER_LEN,
        ACK,
        rwnd,
        0,
        NULL,
        0
    );
    sendToLayer3(send_pkt, HEADER_LEN);
}

void tcp_update_expected_seq(tju_tcp_t* sock, char* pkt) {
    int expected_seq = get_seq(pkt) + get_plen(pkt) - get_hlen(pkt);
    sock->window.wnd_recv->expect_seq = expected_seq;
}

/*
=================================================
====================可靠传输========================
=================================================
*/

/**
 * 检验和
 **/

int tcp_check(tju_packet_t* pkt) {
    unsigned short cksum = tcp_compute_checksum(pkt);
    if(pkt->header.checksum != cksum) {
        printf("checksum error.\n");
        return FALSE;
    }
    // 这里也需要去检查ack

    return TRUE;
}

// 检查序列号，此时需要区分发送方和接收方。
int tcp_check_seq(tju_packet_t* pkt, tju_tcp_t* sock) {
    // 首先查看收到的是不是ACK，若是ACK则查看seqnum是否正确，否则是
    // 传输的分组，直接放入队列中。
    if(pkt->header.flags == ACK) {
        // 发送方收到ACK
        if(pkt->header.seq_num == sock->window.wnd_recv->expect_seq) {
            return 0;
        }else {
            return -1;
        }
    }else {
        // 接收方收到发送方发送来的pakcet，此时应当检查发送来的ack是否与expected_seq一致
        // 一致则将其放入缓冲区中，否则丢弃或者存起来
        if(pkt->header.seq_num == sock->window.wnd_recv->expect_seq) {
            
        }
    }
}

static unsigned short tcp_compute_checksum(tju_packet_t* pkt) {
    unsigned short cksum = 0;
    cksum += (pkt->header.source_port >> 16) & 0xFFFF;
    cksum += (pkt->header.source_port & 0xFFFF);

    cksum += (pkt->header.destination_port >> 16) & 0xFFFF;
    cksum += (pkt->header.destination_port & 0xFFFF);

    cksum += pkt->header.seq_num;
    cksum += pkt->header.ack_num;
    cksum += htons(pkt->header.hlen);
    cksum += htons(pkt->header.plen);
    cksum += pkt->header.flags;
    cksum += pkt->header.advertised_window;
    cksum += pkt->header.ext;

    int data_len = pkt->header.plen - pkt->header.hlen;
    char* data = pkt->data;
    while(data_len > 0) {
        cksum += *data;
        data += 1;
        data_len -= sizeof(char);
    }
    cksum = (cksum >> 16) + (cksum & 0xFFFF);
    cksum = ~cksum;
    return (unsigned short)cksum;
}

void set_checksum(tju_packet_t* pkt) {
    unsigned short cksum = tcp_compute_checksum(pkt);
    pkt->header.checksum = cksum;
}

/**
 * GBN
 **/

// 通过GBN将发送缓冲区的内容发送给下层协议
_Noreturn void* tcp_send_stream(void* arg) {
    tju_tcp_t* sock = (tju_tcp_t*)arg;
    // 监听缓冲区中的数据
    for(;;) {
        if(sock->sending_len > 0) {
            int improve_flag=improve_send_wnd(sock);
            if(!improve_flag) continue;
            int wnd_size=min_among_3(sock->cwnd,sock->window.wnd_send->rwnd,sock->sending_len);

            pthread_mutex_lock(&sock->send_lock);
            int window_left = sock->window.wnd_send->window_size 
            - (sock->window.wnd_send->nextseq - sock->window.wnd_send->base);

            int len = min(wnd_size, window_left);
            char* buf = (char*)malloc(len);
            memcpy(buf, sock->sending_buf, len);

            int left_size = sock->sending_len - len;
            if(left_size > 0) {
                // 若有剩余数据则向前拷贝
                memcpy(sock->sending_buf, sock->sending_buf + len, left_size);
            }
            // 更新socket发送缓冲区的长度
            sock->sending_len -= len;
            pthread_mutex_unlock(&sock->send_lock);

            // GBN 处理过程
            uint16_t plen = DEFAULT_HEADER_LEN + len;
            uint32_t seq = sock->window.wnd_send->nextseq;
            uint32_t base = sock->window.wnd_send->base;
            uint16_t window_size = sock->window.wnd_send->window_size;

            if(seq < base + window_size) {
                // 发送数据
                uint16_t adv_wnd=TCP_SEND_BUFFER_SIZE-sock->received_len;
                char* ptr = sock->window.wnd_send->send_windows + sock->window.wnd_send->nextseq;
                memcpy(ptr, buf, len);
                char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
                    DEFAULT_HEADER_LEN, plen, NO_FLAG, adv_wnd, 0, buf, len);
                printf("[发送数据] 窗口大小: %d\n" , adv_wnd);
                sendToLayer3(msg, plen);
            }
            if(base == seq) {
                // 开始计时
                tcp_start_timer(sock);
            }
            // 更新nextseq的值
            sock->window.wnd_send->nextseq += len;
            // 更新接受窗口的值
        }else {
            // 休息，防止发送线程不断轮询造成CPU负载过重
            sleep(1);
        }
    }
}


/*
=============================================================
========================流量控制==============================
=============================================================
*/

void load_data_to_sending_buffer(tju_tcp_t *sock, const void *buffer, int len) {
    while(pthread_mutex_lock(&(sock->send_lock)) != 0);
    // 判断是否有足够空间存储数据
    if(sock->sending_len+len>TCP_RECVWN_SIZE){
        printf("error: too large data load to sending buffer.");
        exit(-1);
    }
    //将数据拷贝进入buffer
    memcpy(sock->sending_buf+sock->sending_len,buffer,len);
    //更新sending_buffer的大小
    sock->sending_len+=len;
    //存储这个data的大小
    // push(sock->sending_item_flag,MAX_SENDING_ITEM_NUM,len);
    pthread_mutex_unlock(&(sock->recv_lock));
}

//根据sending len自动切分tcp data发送buffer中内容到layer3,最后一个包的

int improve_send_wnd(tju_tcp_t* sock){
    float rwnd= (float)sock->window.wnd_send->rwnd;
    float data_on_way=(float)sock->window.wnd_send->nextseq-sock->window.wnd_send->base;
    if((rwnd-data_on_way)/rwnd<IMPROVED_WINDOW_THRESHOLD){
        return 0;
    }else{
        return 1;
    }
}

void sending_buffer_to_layer3(tju_tcp_t *sock,int len,int push_bit_flag){
    //#TODO ! some params of create_packet_buf are WRONG for a complete implementation, FIX it for a later time.
    // 完整包数
    int full_pkt_nums=len/MAX_DLEN;
    //多出来的
    int left_data_len=len%MAX_DLEN;
    for(int i=0;i<full_pkt_nums;i++){
        char * send_data;
        send_data=malloc(MAX_DLEN);
        memcpy(send_data,sock->sending_buf+i*MAX_DLEN,MAX_DLEN);
        char* msg;
        // calculate the right seq ack
        uint32_t seq = 0;
        uint32_t ack=0;
        uint16_t plen = DEFAULT_HEADER_LEN + MAX_DLEN;
        if(i==full_pkt_nums-1&&left_data_len==0){
            msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, ack,
                                    DEFAULT_HEADER_LEN, plen, PSH, 2000, 0, send_data, MAX_DLEN);
        }else{
            msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, ack,
                                    DEFAULT_HEADER_LEN, plen, NO_FLAG, 2000, 0, send_data, MAX_DLEN);
        }
        sendToLayer3(msg, plen);
    }
    if(left_data_len!=0){
        char * send_data;
        send_data = malloc(len);
        memcpy(send_data,sock->sending_buf+full_pkt_nums*MAX_DLEN,left_data_len);
        char* msg;
        // calculate the right seq ack
        uint32_t seq = 0;
        uint32_t ack=0;
        uint16_t plen = DEFAULT_HEADER_LEN + left_data_len;
        msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, ack,
                                DEFAULT_HEADER_LEN, plen, PSH, 2000, 0, send_data, left_data_len);
        sendToLayer3(msg, plen);
    }
    while(pthread_mutex_lock(&(sock->send_lock)) != 0);
    memcpy(sock->sending_buf,sock->sending_buf+len,sock->sending_len-len);
    sock->sending_len-=len;
    pthread_mutex_unlock(&(sock->recv_lock));
}

void keep_alive(tju_tcp_t *sock){
    uint32_t seq = sock->window.wnd_send->nextseq;
    char* buf = (char*)malloc(0);
    int plen=HEADER_LEN;
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0,
                                  DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, buf, 0);
    sendToLayer3(msg, plen);
}
