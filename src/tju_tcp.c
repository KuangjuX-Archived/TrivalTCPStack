#include "tju_tcp.h"
#include "queue.h"


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

    sock->window.wnd_recv = NULL;
    sock->window.wnd_recv = NULL;

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
    printf("Try to connect......\n");
    conn_sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    memcpy(conn_sock, listen_sock, sizeof(tju_tcp_t));

    // 如果全连接队列为空，则阻塞等待
    printf("Blocking...\n");
    while(is_empty(accept_socks)){}

    printf("Find a socket in accept_socks.\n");
    tju_sock_addr local_addr, remote_addr;
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    // 从全连接队列中取出第一个连接socket
    int status = pop(accept_socks, sock);
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
    int hashval = cal_hash(local_addr.ip, local_addr.port, remote_addr.ip, remote_addr.port);
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
    printf("Start connecting......\n");
    sock->established_remote_addr = target_addr;

    tju_sock_addr local_addr;
    local_addr.ip = inet_network("10.0.0.2");
    local_addr.port = 5678; // 连接方进行connect连接的时候 内核中是随机分配一个可用的端口
    sock->established_local_addr = local_addr;

    // 修改socket的状态
    sock->state = SYN_SENT;

    // 将待连接socket存储入哈希表中
    int hashval = cal_hash(
        local_addr.ip,
        local_addr.port,
        target_addr.ip,
        target_addr.port
    );
    connect_socks[hashval] = sock;

    // 创建客户端socket并将其加入到哈希表中
    // 这里发送SYN_SENT packet向服务端发送请求
    char* send_pkt = create_packet_buf(
        local_addr.port,
        target_addr.port,
        0,
        0,
        20,
        20,
        SYN_SENT,
        0,
        0,
        NULL,
        0
    );
    sendToLayer3(send_pkt, 20);
    printf("send SYN_SENT to layer3.\n");

    // 这里阻塞直到socket的状态变为ESTABLISHED
    printf("Connection block......\n");
    while(sock->state != ESTABLISHED){}

    // 将连接后的socket放入哈希表中
    hashval = cal_hash(
        local_addr.ip, 
        local_addr.port, 
        sock->established_remote_addr.ip, 
        sock->established_remote_addr.port
    );
    established_socks[hashval] = sock;

    // 恢复连接表
    connect_socks[hashval] = NULL;

    printf("Client connect success.\n");
    return 0;
}

int tju_send(tju_tcp_t* sock, const void *buffer, int len){
    // 这里当然不能直接简单地调用sendToLayer3
    char* data = malloc(len);
    memcpy(data, buffer, len);

    char* msg;
    uint32_t seq = 464;
    uint16_t plen = DEFAULT_HEADER_LEN + len;

    msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0, 
              DEFAULT_HEADER_LEN, plen, NO_FLAG, 1, 0, data, len);

    sendToLayer3(msg, plen);
    
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

int tju_handle_packet(tju_tcp_t* sock, char* pkt){
    
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;
    // 这里要判断去处理三次握手和连接关闭的情况，这是不走缓冲区的过程

    // 把收到的数据放到接受缓冲区
    while(pthread_mutex_lock(&(sock->recv_lock)) != 0); // 加锁

    if(sock->received_buf == NULL){
        sock->received_buf = malloc(data_len);
    }else {
        sock->received_buf = realloc(sock->received_buf, sock->received_len + data_len);
    }
    memcpy(sock->received_buf + sock->received_len, pkt + DEFAULT_HEADER_LEN, data_len);
    sock->received_len += data_len;

    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁


    return 1;
}

int tju_close (tju_tcp_t* sock){
    return 0;
}

int tcp_rcv_state_server(tju_tcp_t* sock, char* pkt, tju_sock_addr* conn_addr) {
    uint8_t flags = get_flags(pkt);
    // 通过socket状态进行处理
    switch (sock->state) {
        case CLOSED:
            // 关闭状态，不能接受任何消息
            printf("closed state can't receive messages.\n");
            return -1;
        case LISTEN:
            // 判断packet flags 是否为 SYN_SENT 并判断ack的值
            if (flags == SYN_SENT) {
                printf("receive client status: SYN_SENT.\n");
                // 第二次握手，服务端修改自己的状态，
                // 并且发送 SYN_RECV 标志的pakcet，
                // 加入到半连接哈希表中（暂时不考虑重置状态）                
                // 这里应当构建客户端socket, 从而可以加入到syns_socks中
                tju_tcp_t* conn_sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
                conn_sock->bind_addr.ip = conn_addr->ip;
                conn_sock->bind_addr.port = conn_addr->port;
                printf("Create client socket.\n");

                // 修改服务端socket的状态
                sock->state = SYN_SENT;

                // 加入到半连接哈希表中
                int index = push(syns_socks, sock);
                if (index < 0) {
                    printf("fail to push syns_socks.\n");
                    return -1;
                }
                printf("Push syns_socks.\n");

                // 将syn_id存储进服务器socket中
                sock->saved_syn = index;
                // 创建带有状态的packet
                char* send_pkt;
                int len = build_state_pkt(pkt, &send_pkt, SYN_RECV);
                // 发送packet到客户端
                sendToLayer3(send_pkt, len);
                printf("send SYN_RECV to layer3.\n");
                return 0;
            } else {
                // 当flags不是SYN_SENT，暂时忽略
                printf("TrivialTCP should receive SYN_SENT pakcet.\n");
                return -1;
            }
        case SYN_SENT:
            if (flags == ESTABLISHED) {
                printf("Receive client status: ESTABLISHED.\n");
                // 第三次握手，服务端发送ACK报文， 服务端将自己的socket变为ESTABLISHED，
                // 从syns_socks取出对应的socket并加入到accept_socks中
                sock->state = ESTABLISHED;
                // 获取半连接队列的id
                int index = sock->saved_syn;
                // 获取半连接socket
                tju_tcp_t* conn_sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
                queue_remove(syns_socks, conn_sock, index);
                
                // 将半连接socket放到全连接socket中
                conn_sock->state = ESTABLISHED;
                push(accept_socks, sock);
                printf("Push socket into accept socks.\n");
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
                printf("Receive server status SYN_SENT.\n");
                // 随便编的seq
                int seq = get_seq(pkt) + 464;
                int ack = get_seq(pkt) + 1;
                // 头部长度
                int len = 20;
                // 构建packet发送给服务端
                char* send_pkt = create_packet_buf(
                    sock->established_local_addr.port,
                    conn_sock->port,
                    seq,
                    ack,
                    20,
                    20,
                    ESTABLISHED,
                    0,
                    0,
                    NULL,
                    0
                );
                sendToLayer3(send_pkt, 20);
                // 将socket的状态变为ESTABLISHED
                sock->state = ESTABLISHED;
                return 0;
            }else {
                printf("error status.\n");
                return -1;
            }
        default:
            printf("Unresolved status: %d\n", flags);
            return -1;
    }
}