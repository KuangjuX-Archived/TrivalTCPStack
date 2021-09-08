#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "api.h"
#include "timer.h"
#include "sockqueue.h"
#include "tcp_manager.h"

void* tcp_send_stream(void* arg);
void sendToLayer3(char* packet_buf, int packet_len);
int cal_hash(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port);
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
tju_tcp_t* tcp_socket(){
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    sock->state = CLOSED;
    
    pthread_mutex_init(&sock->send_lock, NULL);
    sock->sending_buf = (char*)malloc(TCP_SEND_BUFFER_SIZE);
    sock->sending_len = 0;
    sock->sending_capacity = TCP_SEND_BUFFER_SIZE;

    pthread_mutex_init(&sock->recv_lock, NULL);
    sock->received_buf = (char*)malloc(TCP_RECV_BUFFER_SIZE);
    sock->received_len = 0;
    sock->received_capacity = TCP_RECV_BUFFER_SIZE;
    
    if(pthread_cond_init(&sock->wait_cond, NULL) != 0){
        perror("ERROR condition variable not set\n");
        exit(-1);
    }

    // 初始化窗口
    sock->window.wnd_recv = (receiver_window_t*)malloc(sizeof(receiver_window_t));
    sock->window.wnd_send = (sender_window_t*)malloc(sizeof(sender_window_t));
    sock->window.wnd_send->send_windows = (char*)malloc(TCP_SEND_WINDOW_SIZE);
    sock->window.wnd_recv->receiver_window = (char*)malloc(TCP_RECV_WINDOW_SIZE);
    sock->window.wnd_send->window_size = TCP_SEND_WINDOW_SIZE;
    sock->window.wnd_send->rwnd = 1 * SMSS;

    // 初始化定时器及回调函数
    tcp_init_timer(sock, tcp_write_timer_handler);
    pthread_mutex_init(&sock->signal_lock, NULL);
    sock->interrupt_signal = 0;

    // 初始化半连接队列和全连接队列
    sockqueue_init(&sock->syn_queue);
    sockqueue_init(&sock->accept_queue);

    //初始化拥塞控制相关
    sock->ssthresh = IW;
    sock->cwnd = SMSS;
    sock->con_status = SLOW_START;

    // 初始化监听socket的状态
    for(int i = 0; i < MAX_SOCK_SIZE; i++) {
        sock->listen_state[i] = CLOSED;
    }

    return sock;
}

/*
绑定监听的地址 包括ip和端口
*/
int tcp_bind(tju_tcp_t* sock, tju_sock_addr bind_addr) {
    sock->bind_addr = bind_addr;
    return 0;
}

/*
被动打开 监听bind的地址和端口
设置socket的状态为LISTEN
注册该socket到内核的监听socket哈希表
*/
int tcp_listen(tju_tcp_t* sock){
    sock->state = LISTEN;
    int hashval = cal_hash(sock->bind_addr.ip, sock->bind_addr.port, 0, 0);
    // 获取tcp_manager并且将socket存储入监听队列中
    tcp_manager_t* tcp_manager = get_tcp_manager();
    tcp_manager->listen_queue[hashval] = sock;
    // 初始化所有socket状态
    for(int i = 0; i < MAX_SOCK_SIZE; i++) {
        tcp_manager->listen_queue[hashval]->listen_state[i] = LISTEN;
    }
    // 更新当前tcp状态
    tcp_manager->is_server = 1;
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
    while(sockqueue_is_empty(listen_sock->accept_queue)){}

    tju_sock_addr local_addr, remote_addr;
    tju_tcp_t* sock = (tju_tcp_t*)malloc(sizeof(tju_tcp_t));
    // 从全连接队列中取出第一个连接socket
    int status = sockqueue_pop(listen_sock->accept_queue, sock);

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
    // established_socks[hashval] = conn_sock;
    // listen_sock->established_queue[hashval] = conn_sock;
    tcp_manager_t* tcp_manager = get_tcp_manager();
    tcp_manager->established_queue[hashval] = conn_sock;
    
    conn_sock->window.wnd_recv->expect_seq = 0;
    conn_sock->window.wnd_send->base = 0;
    conn_sock->window.wnd_send->nextseq = 0;
    pthread_create(&conn_sock->send_thread, NULL, tcp_send_stream, (void*)conn_sock);
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

    // 初始化ack和seq
    uint32_t ack = 0;
    uint32_t seq = 0;

    // 修改tcp_manager的状态
    tcp_manager_t* tcp_manager = get_tcp_manager();
    tcp_manager->is_server = 0;
    // 将待连接socket储存起来
    int hashval = cal_hash(local_addr.ip, local_addr.port, target_addr.ip, target_addr.port);
    tcp_manager->connect_sock[hashval] = sock;

    // 创建客户端socket并将其加入到哈希表中
    // 这里发送SYN_SENT packet向服务端发送请求
    uint16_t adv_wnd = TCP_RECV_BUFFER_SIZE-sock->received_len;
    char* send_pkt = create_packet_buf(
        local_addr.port,
        target_addr.port,
        seq,
        ack,
        HEADER_LEN,
        HEADER_LEN,
        SYN,
        adv_wnd,
        0,
        NULL,
        0
    );
    tcp_start_timer(sock);
    sendToLayer3(send_pkt, HEADER_LEN);

    // 这里阻塞直到socket的状态变为ESTABLISHED
    printf("Wait connection......\n");
    while(sock->state != ESTABLISHED){}
    pthread_create(&sock->send_thread, NULL, tcp_send_stream, (void*)sock);

    // 将连接后的socket放入哈希表中
    hashval = cal_hash(
        local_addr.ip, 
        local_addr.port, 
        sock->established_remote_addr.ip, 
        sock->established_remote_addr.port
    );
    // established_socks[hashval] = sock;
    // sock->established_queue[hashval] = sock;
    // tcp_manager_t* tcp_manager = get_tcp_manager();
    tcp_manager->connect_sock[hashval] = sock;
    sock->window.wnd_recv->expect_seq = 0;
    sock->window.wnd_send->base = 0;
    sock->window.wnd_send->nextseq = 0;

    printf("Client connect success.\n");
    return 0;
}

int tcp_send(tju_tcp_t* sock, const void *buffer, int len){
    char* data = malloc(len);
    memcpy(data, buffer, len);

    // 将packet加入到sending_buf中
    // 获取锁
    pthread_mutex_lock(&sock->send_lock);
    if(sock->sending_len + len <= sock->sending_capacity) {
        // 将packet按字节push到缓冲区中
        char* ptr = sock->sending_buf + sock->sending_len;
        memcpy(ptr, data, len);
        sock->sending_len += len;
    }else {
        printf("Sending buffer is full.\n");
        return -1;
    }
    // 释放锁
    pthread_mutex_unlock(&sock->send_lock);
    
    return 0;
}

int tcp_recv(tju_tcp_t* sock, void *buffer, int len){
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
        memcpy(sock->received_buf,sock->received_buf+read_len,sock->received_len-read_len);
        sock->received_len -= read_len;
    }else{
        sock->received_len = 0;
    }
    pthread_mutex_unlock(&(sock->recv_lock)); // 解锁

    return 0;
}

// 关闭连接，向远程发送FIN pakcet，等待资源释放
int tcp_close (tju_tcp_t* sock) {
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
    while(sock->state != CLOSED) {}
}
