#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "kernel.h"
#include "sockqueue.h"
#include "tcp_manager.h"

// struct tcp_manager_t;
// void tcp_manager_init();
// struct tcp_manager_t* get_tcp_manager();

/*
==========================================================
================以下为模拟内核执行情况的函数调用================
==========================================================
*/

// 建立连接后接收分组需要通过tju_handle_packet，这里需要判断ack和seq 
int tju_handle_packet(tju_tcp_t* sock, char* pkt) {
    uint8_t flag = get_flags(pkt);
    uint32_t seq = get_seq(pkt);
    uint32_t adv_wnd= get_advertised_window(pkt);
    sock->window.wnd_send->rwnd=adv_wnd;
    // printf("package flag %d \n",flag);
    if(flag == HEAD){
        // printf("[RECEIVE] only head.\n");
        back_only_header(sock);
        return 0;
    }

    if(flag==BACKHEAD){
        // printf("BACK HEAD [RECEIVE] only head.\n");
        return 0;
    }

    if(flag == ACK) {
        handle_success_ack(sock);
        printf("[收到ACK] 接收到ACK.\n");
        // 此处为发送方，收到接收方传来的ACK
        // 需要检查是否有“捎带”的数据
        uint32_t seq = get_seq(pkt);
        printf("[收到ACK] 序列号 seq 为: %d.\n", seq);
        sock->window.wnd_send->base = seq + get_plen(pkt) - get_hlen(pkt);
        uint32_t base = sock->window.wnd_send->base;
        uint32_t next_seq = sock->window.wnd_send->nextseq;
        printf("[收到ACK] 窗口基准 base 为: %d, 下一个要发送的分组序列号 next_seq 为: %d.\n", base, next_seq);
        if(base == next_seq) {
            tcp_stop_timer(sock);
        }else {
            tcp_start_timer(sock);
        }

        if(get_plen(pkt) < get_hlen(pkt)) {
            printf(RED "[接收方] 收到非法的 pakcet len, 退出程序.\n" RESET);
            exit(0);
        }
        // 此时有"捎带"数据，应当继续执行将"捎带"数据存入到received_buf中

    }else {
        // 此处为接收方，收到发送方传来的ACK
        // 这里必须保证seq和expected_seq相同，否则是失序的packet
        uint32_t expected_seq = sock->window.wnd_recv->expect_seq;
        printf("[收到分组] 序列号 seq 为: %d.\n", seq);
        printf("[收到分组] 期待的序列号 expected_seq 为: %d.\n", expected_seq);
        if(seq > expected_seq) {
            // 接受到了之后的seq
            // 选择丢弃或者存起来(选择重传)

            // 这里先丢弃
            printf("[处理分组] 失序的序列号.\n");
            return 0;
        }else if(seq == expected_seq) {
            // 向对方发送ACK
            // 修改自己的expected_seq
            // 更新expected_seq
            int len = get_plen(pkt) - get_hlen(pkt);
            sock->window.wnd_recv->expect_seq += len;
            // 继续执行，接受数据
        }else if(seq < expected_seq) {
            printf("[处理分组] 无效的序列号.\n");
            return -1;
        }
    }
    uint32_t data_len = get_plen(pkt) - DEFAULT_HEADER_LEN;

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
    tcp_send_ack(sock);

    return 0;
}

/*
模拟Linux内核收到一份TCP报文的处理函数
*/
int onTCPPocket(char* pkt){
    // 当我们收到TCP包时 包中 源IP 源端口 是发送方的 也就是我们眼里的 远程(remote) IP和端口
    uint16_t remote_port = get_src(pkt);
    uint16_t local_port = get_dst(pkt);
    // printf("[流量控制] 获取对方窗口大小为: %d.\n", get_advertised_window(pkt));
    // remote ip 和 local ip 是读IP 数据包得到的 仿真的话这里直接根据hostname判断
    // 获取是server还是client
    int is_server;
    char hostname[8];
    gethostname(hostname, 8);
    uint32_t remote_ip, local_ip;
    if(strcmp(hostname,"server")==0){ // 自己是服务端 远端就是客户端
        local_ip = inet_network("10.0.0.3");
        remote_ip = inet_network("10.0.0.2");
        is_server = 1;
    }else if(strcmp(hostname,"client")==0){ // 自己是客户端 远端就是服务端 
        local_ip = inet_network("10.0.0.2");
        remote_ip = inet_network("10.0.0.3");
        is_server = 0;
    }

    tju_packet_t* packet = buf_to_packet(pkt);
    if(!tcp_check(packet)) {
        printf("tcp check error.\n");
        return -1;
    }
    if(packet->data != NULL) {
        free(packet->data);
    }
    free(packet);   

    int hashval;
    // 获得tcp_manager
    tcp_manager_t* tcp_manager = get_tcp_manager();
    hashval = cal_hash(local_ip, local_port, 0, 0);
    tju_tcp_t* listen_sock = tcp_manager->listen_queue[hashval];
    
    // 首先查找已经建立连接的socket哈希表
    // 根据4个ip port 组成四元组 查找有没有已经建立连接的socket
    hashval = cal_hash(local_ip, local_port, remote_ip, remote_port);
    tju_tcp_t* connect_sock = tcp_manager->connect_sock[hashval];

    if(
        tcp_manager->is_server && 
        tcp_manager->established_queue[hashval] != NULL &&
        (is_fin(pkt) || tcp_manager->established_queue[hashval]->state != ESTABLISHED)
    ) {
        return tcp_state_close(tcp_manager->established_queue[hashval], pkt);
    
    }else if(
        !tcp_manager->is_server && 
        connect_sock != NULL &&
        (is_fin(pkt) || (connect_sock->state != ESTABLISHED && connect_sock->state != SYN_SENT))
        ) {
            return tcp_state_close(connect_sock, pkt);
    }else if(tcp_manager->is_server && tcp_manager->established_queue[hashval] != NULL) {
        return tju_handle_packet(tcp_manager->established_queue[hashval], pkt);
    }else if(!tcp_manager->is_server && connect_sock->state == ESTABLISHED) {
//        printf("[流量控制] 我的窗口大小为: %d.\n", connect_sock->received_len);
        return tju_handle_packet(connect_sock, pkt);
    }

    tju_sock_addr conn_addr;
    conn_addr.ip = remote_ip;
    conn_addr.port = remote_port;
    

    hashval = cal_hash(local_ip, local_port, 0, 0);
    // 没有的话再查找监听中的socket哈希表
    if (tcp_manager->listen_queue[hashval] != NULL && is_server) {
        // 监听的socket只有本地监听ip和端口 没有远端
        return tcp_rcv_state_server(tcp_manager->listen_queue[hashval], pkt, &conn_addr);
    }

    hashval = cal_hash(local_ip, local_port, remote_ip, remote_port);
    if (tcp_manager->connect_sock[hashval] != NULL && !is_server) {
        return tcp_rcv_state_client(tcp_manager->connect_sock[hashval], pkt, &conn_addr);
    }

    // 都没找到 丢掉数据包
    printf("找不到能够处理该TCP数据包的socket, 丢弃该数据包\n");
    return -1;
}



/*
以用户填写的TCP报文为参数
根据用户填写的TCP的目的IP和目的端口,向该地址发送数据报
不可以修改此函数实现
*/
void sendToLayer3(char* packet_buf, int packet_len){
    if (packet_len > MAX_LEN){
        printf("ERROR: 不能发送超过 MAX_LEN 长度的packet, 防止IP层进行分片\n");
        return;
    }

    // 获取hostname 根据hostname 判断是客户端还是服务端
    char hostname[8];
    gethostname(hostname, 8);

    struct sockaddr_in conn;
    conn.sin_family      = AF_INET;            
    conn.sin_port        = htons(20218);
    int rst;
    if(strcmp(hostname,"server")==0){
        conn.sin_addr.s_addr = inet_addr("10.0.0.2");
        rst = sendto(BACKEND_UDPSOCKET_ID, packet_buf, packet_len, 0, (struct sockaddr*)&conn, sizeof(conn));
    }else if(strcmp(hostname,"client")==0){       
        conn.sin_addr.s_addr = inet_addr("10.0.0.3");
        rst = sendto(BACKEND_UDPSOCKET_ID, packet_buf, packet_len, 0, (struct sockaddr*)&conn, sizeof(conn));
    }else{
        printf("hostname: %s\n", hostname);
        printf("请不要改动hostname...\n");
        exit(-1);
    }
}

/*
 仿真接受数据线程
 不断调用server或cliet监听在20218端口的UDPsocket的recvfrom
 一旦收到了大于TCPheader长度的数据 
 则接受整个TCP包并调用onTCPPocket()
*/
void* receive_thread(void* arg){

    char hdr[DEFAULT_HEADER_LEN];
    char* pkt;

    uint32_t plen = 0, buf_size = 0, n = 0;
    int len;

    struct sockaddr_in from_addr;
    int from_addr_size = sizeof(from_addr);

    while(1) {
        // MSG_PEEK 表示看一眼 不会把数据从缓冲区删除
        len = recvfrom(BACKEND_UDPSOCKET_ID, hdr, DEFAULT_HEADER_LEN, MSG_PEEK, (struct sockaddr *)&from_addr, &from_addr_size);
        // 一旦收到了大于header长度的数据 则接受整个TCP包
        if(len >= DEFAULT_HEADER_LEN) {
            plen = get_plen(hdr); 
            pkt = (char*)malloc(plen);
            buf_size = 0;
            while(buf_size < plen) { // 直到接收到 plen 长度的数据 接受的数据全部存在pkt中
                n = recvfrom(BACKEND_UDPSOCKET_ID, pkt + buf_size, plen - buf_size, NO_FLAG, (struct sockaddr *)&from_addr, &from_addr_size);
                buf_size = buf_size + n;
            }
            // 通知内核收到一个完整的TCP报文
            if (onTCPPocket(pkt) < 0) {
                // 根据 TCP 标准，此时应当向对方发送RST标志位
                // printf("[内核线程] 向对方发送RST标志位.\n");
            }
            free(pkt);
        }
    }
}

/*
 开启仿真, 运行起后台线程

 不论是server还是client
 都创建一个UDP socket 监听在20218端口
 然后创建新线程 不断调用该socket的recvfrom
*/
void startSimulation(){
    // 对于内核 初始化监听socket哈希表和建立连接socket哈希表
    // int index;
    // for(index=0;index<MAX_SOCK;index++){
    //     listen_socks[index] = NULL;
    //     established_socks[index] = NULL;
    //     connect_sock = NULL;
    // }
    // // 初始化半连接队列和全连接队列
    // sockqueue_init(&syns_socks);
    // sockqueue_init(&accept_socks);

    // 初始化tcp_manager
    tcp_manager_init();


    // 获取hostname 
    char hostname[8];
    gethostname(hostname, 8);
    // printf("startSimulation on hostname: %s\n", hostname);

    BACKEND_UDPSOCKET_ID = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (BACKEND_UDPSOCKET_ID < 0){
        printf("ERROR opening socket");
        exit(-1);
    }

    // 设置socket选项 SO_REUSEADDR = 1 
    // 意思是 允许绑定本地地址冲突 和 改变了系统对处于TIME_WAIT状态的socket的看待方式 
    int optval = 1;
    setsockopt(BACKEND_UDPSOCKET_ID, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    struct sockaddr_in conn;
    memset(&conn, 0, sizeof(conn)); 
    conn.sin_family = AF_INET;
    conn.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY = 0.0.0.0
    conn.sin_port = htons((unsigned short)20218);

    if (bind(BACKEND_UDPSOCKET_ID, (struct sockaddr *) &conn, sizeof(conn)) < 0){
        printf("ERROR on binding");
        exit(-1);
    }

    pthread_t thread_id = 1001;
    int rst = pthread_create(&thread_id, NULL, receive_thread, (void*)(&BACKEND_UDPSOCKET_ID));
    if (rst < 0){
        printf("ERROR open thread");
        exit(-1); 
    }
    return;
}

int cal_hash(uint32_t local_ip, uint16_t local_port, uint32_t remote_ip, uint16_t remote_port) {
    // 实际上肯定不是这么算的
    return ((int)local_ip+(int)local_port+(int)remote_ip+(int)remote_port) % MAX_SOCK_SIZE;
}

/*
=======================================================
====================拥塞控制相关函数===================
=======================================================
*/

void handle_success_ack(tju_tcp_t* sock){
    if(sock->con_status==SLOW_START){
        sock->cwnd=sock->cwnd+SMSS;
        if(sock->cwnd>sock->ssthresh){
            sock->con_status=CONGESTION_AVOIDANCE;
        }
    }else if(sock->con_status==CONGESTION_AVOIDANCE){
         sock->cwnd=sock->cwnd+SMSS*(SMSS/sock->cwnd);
    }else if(sock->con_status==FAST_RECOVERY){
        sock->cwnd=sock->cwnd+SMSS;
    }else{
        printf("handle_success_ack 出现未定义行为\n");
    }
}

/*
=======================================================
====================流量控制相关函数===================
=======================================================
*/

void back_only_header(tju_tcp_t* sock){
    // printf("%d %d \n",sock->received_len,TCP_RECV_BUFFER_SIZE-sock->received_len);
    uint16_t adv_wnd = TCP_RECV_BUFFER_SIZE-sock->received_len;
    uint16_t plen = DEFAULT_HEADER_LEN;
    uint32_t seq = sock->window.wnd_send->nextseq;
    char* buf;
    char* msg = create_packet_buf(sock->established_local_addr.port, sock->established_remote_addr.port, seq, 0,
                                  DEFAULT_HEADER_LEN, plen, BACKHEAD, adv_wnd, 0, buf, 0);
    // printf("[发送 BACK ONLY_HEADER] 窗口大小: %d received len: %d\n" , adv_wnd,sock->received_len);
    sendToLayer3(msg, plen);
}