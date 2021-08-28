#include "tju_tcp.h"
#include "sockqueue.h"
#include "timer.h"

/*
=======================================================
====================系统调用API函数如下===================
=======================================================
*/

/*
创建 TCP socket 
初始化对应的结构体
设置初始状态为 CLOSED
*/
tju_tcp_t* tju_socket(){
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    sock->state = CLOSED;
    
    pthread_mutex_init(&(sock->send_lock), NULL);
    sock->sending_buf = NULL;
    sock->sending_len = 0;

    pthread_mutex_init(&(sock->recv_lock), NULL);
    sock->received_buf = NULL;
    sock->received_len = 0;
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){
        perror("ERROR condition variable not set\n");
        exit(-1);
    }

    // 初始化窗口
    sock->window.wnd_recv = (receiver_window_t*)malloc(sizeof(receiver_window_t));
    sock->window.wnd_send = (sender_window_t*)malloc(sizeof(sender_window_t));

    // 初始化定时器及回调函数
    tcp_init_timer(sock, tcp_write_timer_handler);

    return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr) {
    sock->bind_addr = bind_addr;
    return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
*/
int tju_listen(tju_tcp_t* sock){
    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    listen_socks[hashval] = sock;
    return 0;
}

/*
接受连接 
返回与客户端通信用的socket
这里返回的socket一定是已经完成3次握手建立了连接的socket
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
accept API:
int accept(int sockfd, struct sockaddr *restrict addr,
        socklen_t *restrict addrlen);
*/

int tcp_accept(tju_tcp_t* listen_sock, tju_tcp_t* conn_sock) {
    memcpy(conn_sock, listen_sock, sizeof(tju_tcp_t));

    // 如果全连接队列为空，则阻塞等待
    printf("Wait connection......\n");
    while(sockqueue_is_empty(accept_socks)){}

    tju_sock_addr local_addr, remote_addr;
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    // 从全连接队列中取出第一个连接socket
    int status = sockqueue_pop(accept_socks, sock);

    if(status < 0) {
        return -1;
    }

    // 为conn_sock更新信息
    local_addr.ip = listen_sock->bind_addr.ip;
    local_addr.port = listen_sock->bind_addr.port;
    remote_addr.ip = sock->bind_addr.ip;
    remote_addr.port = sock->bind_addr.port;

    conn_sock->established_local_addr = local_addr;
    conn_sock->established_remote_addr = remote_addr;
    conn_sock->state = ESTABLISHED;

    // 将客户端socket放入已建立连接的表中
    int hashval = cal_hash(
        local_addr.ip, 
        local_addr.port, 
        remote_addr.ip, 
        remote_addr.port
    );
    established_socks[hashval] = conn_sock;
    printf("Connection established.\n");

    // status code: find a connect socket
    return 0;
}


/*
连接到服务端
该函数以一个socket为参数
调用函数前, 该socket还未建立连接
函数正常返回后, 该socket一定是已经完成了3次握手, 建立了连接
因为只要该函数返回, 用户就可以马上使用该socket进行send和recv
*/
int tcp_connect(tju_tcp_t* sock, tju_sock_addr target_addr) {
    sock->established_remote_addr = target_addr;

    tju_sock_addr local_addr;
    local_addr.ip = inet_network("10.0.0.2");
    local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
    sock->established_local_addr = local_addr;

    // 修改socket的状态
    sock->state = SYN_SENT;

    // 将待连接socket储存起来
    connect_sock = sock;

    // 初始化ack和seq
    uint32_t ack = 0;
    uint32_t seq = 0;

    // 创建客户端socket并将其加入到哈希表中
    // 这里发送SYN_SENT packet向服务端发送请求
    char* send_pkt = create_packet_buf(
        local_addr.port,
        target_addr.port,
        seq,
        ack,
        HEADER_LEN,
        HEADER_LEN,
        SYN_SENT,
        0,
        0,
        NULL,
        0
    );
    sendToLayer3(send_pkt, HEADER_LEN);

    // 这里阻塞直到socket的状态变为ESTABLISHED
    printf("Wait connection......\n");
    while(sock->state != ESTABLISHED){}

    // 将连接后的socket放入哈希表中
    int hashval = cal_hash(
        local_addr.ip, 
        local_addr.port, 
        sock->established_remote_addr.ip, 
        sock->established_remote_addr.port
    );
    established_socks[hashval] = sock;

    printf("Client connect success.\n");
    return 0;
}

int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    // 这里当然不能直接简单地调用sendToLayer3
    char* data = malloc(len);
    memcpy(data, buffer, len);

    char* msg;
    uint16_t plen = DEFAULT_HEADER_LEN + len;
    uint32_t seq = sock->window.wnd_send->nextseq;
    uint32_t base = sock->window.wnd_send->base;
    uint16_t window_size = sock->window.wnd_send->window_size;

    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, data, len);
    tju_packet_t* send_packet = buf_to_packet(msg);
    if(seq < base + window_size) {
        sock->window.wnd_send->send_windows[seq % TCP_SEND_WINDOW_SIZE] = send_packet;
        sendToLayer3(msg, plen);
    }
    if(base == seq) {
        // 开始计时
    }
    sock->window.wnd_send->nextseq += 1;
    
    return 0;
}

int tju_recv(tju_tcp_t* sock, void *buffer, int len){
    while(sock->received_len <= 0){
        // 阻塞
    }
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    int read_len = 0;
    if (sock->received_len >= len){ // 从中读取len长度的数据
        read_len = len;
    }else{
        read_len = sock->received_len; // 读取sock->received_len长度的数据(全读出来)
    }

    memcpy(buffer, sock->received_buf, read_len);

    if(read_len < sock->received_len) { // 还剩下一些
        char* new_buf = malloc(sock->received_len - read_len);
        memcpy(new_buf, sock->received_buf + read_len, sock->received_len - read_len);
        free(sock->received_buf);
        sock->received_len -= read_len;
        sock->received_buf = new_buf;
    }else{
        free(sock->received_buf);
        sock->received_buf = NULL;
        sock->received_len = 0;
    }
    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    return 0;
}

// 关闭连接，向远程发送FIN pakcet，等待资源释放
int tcp_close (tju_tcp_t* sock){
    // 首先应该检查接收队列中的缓冲区是否仍有剩余
    while(sock->received_buf != NULL) {
        // 若仍有剩余则清空
        free(sock->received_buf);
        sock->received_len = 0;
        sock->received_buf = NULL;
    }
    // 修改当前状态
    sock->state = FIN_WAIT_1;
    // 发送FIN分组
    tcp_send_fin(sock);
    // 这里暂时只检查客户端的状态
    while(connect_sock != NULL) {}
}

/*
====================================================
===========以下为辅助函数的实现，仅在该模块被调用=========
====================================================
*/


int tcp_rcv_state_server(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_addr) {
    uint8_t flags = get_flags(pkt);
    // 通过socket状态进行处理
    switch (sock->state) {
        case CLOSED:
            // 关闭状态，不能接受任何消息
            printf("Closed state can't receive messages.\n");
            return -1;
        case LISTEN:
            // 判断packet flags 是否为 SYN_SENT 并判断ack的值
            if (flags == SYN_SENT) {
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
                int index = sockqueue_push(syns_socks, conn_sock);
                if (index < 0) {
                    printf("Fail to push syns_socks.\n");
                    return -1;
                }
                // 将syn_id存储进服务器socket中
                sock->saved_syn = index;
                // 创建带有状态的packet
                char* send_pkt;
                int len = build_state_pkt(pkt, &send_pkt, SYN_RECV);
                // 发送packet到客户端
                sendToLayer3(send_pkt, len);
                return 0;
            } else {
                // 当flags不是SYN_SENT，暂时忽略
                printf("TrivialTCP should receive SYN_SENT pakcet.\n");
                return -1;
            }
        case SYN_SENT:
            if (flags == ESTABLISHED && tcp_check_seq(pkt, sock)) {
                // 第三次握手，服务端发送ACK报文， 服务端将自己的socket变为ESTABLISHED，
                // 从syns_socks取出对应的socket并加入到accept_socks中
                sock->state = ESTABLISHED;

                // 更新expected_seq
                tcp_update_expected_seq(sock, pkt);
                
                // 获取半连接队列的id
                int index = sock->saved_syn;
                // 获取半连接socket
                tju_tcp_t* conn_sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
                sockqueue_remove(syns_socks, conn_sock, index);
                // 将半连接socket放到全连接socket中
                conn_sock->state = ESTABLISHED;
                sockqueue_push(accept_socks, conn_sock);
                return 0;
            } else {
                // 当flags不是ESTABLSHED时，暂时忽略
                printf("TrivialTCP should receive ESTABLISHED packet.\n");
                return -1;
            }
        default:         
            printf("Unresolved status: %d\n", flags);
            return -1;
    }
}

int tcp_rcv_state_client(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_sock) {
    uint8_t flags = get_flags(pkt);
    switch(flags) {
        case SYN_RECV:
            if(sock->state == SYN_SENT) {
                // client首次收到ACK， 更新窗口expected_seq
                tcp_update_expected_seq(sock, pkt);
                // 随便编的seq
                int seq = get_seq(pkt) + 1;
                int ack = get_seq(pkt) + 1;
                // 头部长度
                int len = 20;
                // 构建packet发送给服务端
                char* send_pkt = create_packet_buf(
                    sock->established_local_addr.port,
                    conn_sock->port,
                    seq,
                    ack,
                    HEADER_LEN,
                    HEADER_LEN,
                    ESTABLISHED,
                    0,
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
            printf("Unresolved status: %d\n", flags);
            return -1;
    }
}

// 事实上除了通过判断recv_pkt的flags时也需要检查ack，这里暂时没有实现
int tcp_state_close(tju_tcp_t* local_sock, char* recv_pkt) {
    uint8_t flags = get_flags(recv_pkt);
    switch(local_sock->state) {
        case ESTABLISHED:
            // 服务端接受客户端发起的close请求
            if(flags == (FIN | ACK)) {
                // 将本地socket状态修改为CLOS_WAIT
                local_sock->state = CLOSE_WAIT;
                // 向对方发送ACK
                tcp_send_ack(local_sock);
                
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
                tcp_send_ack(local_sock);
                local_sock->state = TIME_WAIT;
                // 等待2 * MSL 时间，释放socket资源
                sleep(2 * MSL);
                free(local_sock);
                connect_sock = NULL;
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
    int seq = 464;
    int ack = 0;
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

void tcp_send_ack(tju_tcp_t* sock) {
    // 瞎编seq和ack
    uint32_t seq = 464;
    uint32_t ack = 0;
    char* send_pkt = create_packet_buf(
        sock->established_local_addr.port,
        sock->established_remote_addr.port,
        seq,
        ack,
        HEADER_LEN,
        HEADER_LEN,
        ACK,
        0,
        0,
        NULL,
        0
    );
    sendToLayer3(send_pkt, HEADER_LEN);
}

void tcp_update_expected_seq(tju_tcp_t* sock, tju_packet_t* pkt) {
    int expected_seq = get_seq(pkt) + 1;
    sock->window.wnd_recv->expect_seq = expected_seq;
}


void load_data_to_sending_window(tju_tcp_t *sock, const void *buffer, int len) {
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

void calculate_sending_buffer_depend_on_rwnd(tju_tcp_t* sock){
    //    int if_nagle_flag=FALSE;
    // #TODO Nagle alg
    // 两种情况
    if(sock->window.wnd_send->rwnd>=sock->sending_len){
        sending_buffer_to_layer3(sock,sock->sending_len,TRUE);
    }else{
        // #TODO blow code could block the thread, rewrite later time.
        while(sock->sending_len!=0){
            sending_buffer_to_layer3(sock,sock->window.wnd_send->rwnd,TRUE);
            while(pthread_mutex_lock(&(sock->send_lock)) != 0);
            memcpy(sock->sending_buf,sock->sending_buf+sock->window.wnd_send->rwnd,sock->sending_len-sock->window.wnd_send->rwnd);
            sock->sending_len-=sock->window.wnd_send->rwnd;
            pthread_mutex_unlock(&(sock->recv_lock));
        }
    }
}
