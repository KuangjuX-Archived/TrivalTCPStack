#include "tju_tcp.h"
#include <string.h>

int TEST_BACKEND_UDPSOCKET_ID;

int STATE = 0; // 0 还没有收到任何包 1 收到SYN 已发送SYNACK 2 成功返回

time_t start_time,now_time;  
int timer_maxtime  = 0;
int timer_istimeout  = 0;
int quit_timer  = 0;

uint32_t client_first_send_seq; // 客户端第一次SYN传过来的seq 

void reset_timer(int maxtime){
    start_time = time(NULL);
    timer_maxtime = maxtime;
    timer_istimeout = 0;
}

void sendToLayer3Test(char* packet_buf, int packet_len){
    if (packet_len>MAX_LEN){
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
        rst = sendto(TEST_BACKEND_UDPSOCKET_ID, packet_buf, packet_len, 0, (struct sockaddr*)&conn, sizeof(conn));
    }else if(strcmp(hostname,"client")==0){       
        conn.sin_addr.s_addr = inet_addr("10.0.0.1");
        rst = sendto(TEST_BACKEND_UDPSOCKET_ID, packet_buf, packet_len, 0, (struct sockaddr*)&conn, sizeof(conn));
    }else{
        printf("请不要改动hostname...\n");
        exit(-1);
    }
}

void onTCPPocketTest(char* pkt){
    printf("[服务端] 收到一个TCP数据包\n");
    // 获得包的各项数据
    uint16_t pkt_src = get_src(pkt);
    uint16_t pkt_dst = get_dst(pkt);
    uint32_t pkt_seq = get_seq(pkt);
    uint32_t pkt_ack = get_ack(pkt);
    uint32_t pkt_flags = get_flags(pkt);

    // 获取 TCP包的 标志位
    int syn_flag=0, ack_flag=0, fin_flag=0;
    if ( (get_flags(pkt)>>3) & 1 == 1 ){ // SYN 是第四位
        syn_flag = 1;
    }
    if ( (get_flags(pkt)>>2) & 1 == 1 ){ // ACK 是第三位
        ack_flag = 1;
    }
    if ( (get_flags(pkt)>>1) & 1 == 1 ){ // SYN 是第二位
        fin_flag = 1;
    }

    if(STATE==0){
        int success=1;

        // 此时应该接受SYN包
        if (syn_flag!=1){
            printf("[服务端] 接收到的第一个TCP包SYN标志位不为1\n");
            success = 0;
        }

        // SYN 的 dst应该是调用connect时传入的1234
        if (pkt_dst!=1234){
            printf("[服务端] 接收到的SYN的dst不是调用connect时传入的1234\n");
            success = 0;
        }

        if (success){
            printf("[服务端] 客户端发送的第一个SYN报文检验通过\n");
            printf("{{GET SCORE}}\n");
            STATE = 1;

            client_first_send_seq = pkt_seq;
            
            // 继续等待3s 接受ACK 
            reset_timer(3);

            // 发送SYNACK
            char* msg;
            uint32_t seq = 646; // 随机seq
            uint16_t plen = DEFAULT_HEADER_LEN;
            uint32_t send_ack_val = pkt_seq + 1;
            msg = create_packet_buf((uint16_t)1234, pkt_src, 
                                    seq, send_ack_val, 
                                    DEFAULT_HEADER_LEN, plen, SYN_FLAG_MASK|ACK_FLAG_MASK, 32, 0, NULL, 0);
            printf("[服务端] 发送SYNACK 等待客户端第三次握手的ACK\n");
            sendToLayer3Test(msg, plen);
            free(msg);
        }else{
            printf("[服务端] 客户端发送的第一个SYN数据包出现问题, 结束测试\n");
            STATE = -1;
        }
        
    }else if(STATE==1){
        int success=1;

        // 此时应该收到ACK
        if (syn_flag==1||ack_flag!=1){
            printf("[服务端] 接收到的第三次握手的SYN应该为 0 ACK应该为 1\n");
            success = 0;
        }

        // ACK 的 dst 应该是1234
        if (pkt_dst!=1234){
            printf("[服务端] 第三次握手 接收到的ACK的dst不是调用connect时传入的 1234\n");
            success = 0;
        }

        // ACK的ack值应该是服务端发送的SYNACK的seq+1
        if (pkt_ack!=647){
            printf("[服务端] 第三次握手 ACK的ack值应该是服务端发送的SYNACK的 seq+1\n");
            success = 0;   
        }

        if (success){
            reset_timer(15);
            printf("[服务端] 客户端发送的ACK报文检验通过, 成功建立连接\n");
            STATE = 2;
            printf("{{GET SCORE}}\n");
            printf("{{TEST SUCCESS}}\n");
        }else{
            STATE = -1;
            printf("[服务端] 客户端发送的第三次握手ACK数据包出现问题, 结束测试\n");
        }

    }
    return;
}


void* receive_thread_test(void* arg){

    char hdr[DEFAULT_HEADER_LEN];
    char* pkt;

    uint32_t plen = 0, buf_size = 0, n = 0;
    int len;

    struct sockaddr_in from_addr;
    int from_addr_size = sizeof(from_addr);


    while(1) {
        if(timer_istimeout==1){
            quit_timer = 1;
            if (STATE==0){
                printf("[服务端] 未能在规定时间内接收到第一次握手的SYN数据包\n");
            }else if(STATE==1){
                printf("[服务端] 未能在规定时间内接收到第三次握手的ACK数据包\n");
            }
            printf("{{TEST FAILED}}\n");
            break;
        }
        if(STATE==2){
            quit_timer = 1;
            break;
        }
        if(STATE==-1){
            quit_timer = 1;
            printf("{{TEST FAILED}}\n");
            break;
        }
        // MSG_PEEK 表示看一眼 不会把数据从缓冲区删除 
        // NO_WAIT 表示不阻塞 用于计时
        len = recvfrom(TEST_BACKEND_UDPSOCKET_ID, hdr, DEFAULT_HEADER_LEN, MSG_PEEK|MSG_DONTWAIT, (struct sockaddr *)&from_addr, &from_addr_size);
        // 一旦收到了大于header长度的数据 则接受整个TCP包
        if(len >= DEFAULT_HEADER_LEN){
            plen = get_plen(hdr); 
            pkt = malloc(plen);
            buf_size = 0;
            while(buf_size < plen){ // 直到接收到 plen 长度的数据 接受的数据全部存在pkt中
                n = recvfrom(TEST_BACKEND_UDPSOCKET_ID, pkt + buf_size, plen - buf_size, NO_FLAG, (struct sockaddr *)&from_addr, &from_addr_size);
                buf_size = buf_size + n;
            }
            // 收到一个完整的TCP报文
            onTCPPocketTest(pkt);
            free(pkt);
        }
    }
}

void* timer_thread(void* arg){
    while(1){
        if(quit_timer==1){
            break;
        }
        now_time = time(NULL);
        if (difftime(now_time, start_time)>timer_maxtime){
            timer_istimeout = 1;
        }
    }
}

int main(int argc, char **argv) {

    TEST_BACKEND_UDPSOCKET_ID  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (TEST_BACKEND_UDPSOCKET_ID < 0){
        printf("ERROR opening socket\n");
        exit(-1);
    }

    // 设置socket选项 SO_REUSEADDR = 1 
    // 意思是 允许绑定本地地址冲突 和 改变了系统对处于TIME_WAIT状态的socket的看待方式 
    int optval = 1;
    setsockopt(TEST_BACKEND_UDPSOCKET_ID, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    struct sockaddr_in conn;
    memset(&conn, 0, sizeof(conn)); 
    conn.sin_family = AF_INET;
    conn.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY = 0.0.0.0
    conn.sin_port = htons((unsigned short)20218);

    if (bind(TEST_BACKEND_UDPSOCKET_ID, (struct sockaddr *) &conn, sizeof(conn)) < 0){
        printf("ERROR on binding\n");
        exit(-1);
    }

    pthread_t recv_thread_id = 998, timer_thread_id = 997;
    STATE = 0;
    int rst = pthread_create(&recv_thread_id, NULL, receive_thread_test, (void*)(&TEST_BACKEND_UDPSOCKET_ID));
    if (rst<0){
        printf("ERROR open thread\n");
        exit(-1); 
    }
    rst = pthread_create(&timer_thread_id, NULL, timer_thread, NULL);
    if (rst<0){
        printf("ERROR open thread\n");
        exit(-1); 
    }

    // 等待3s接受第一个SYN
    reset_timer(3);

    pthread_join(recv_thread_id, NULL); 
    pthread_join(timer_thread_id, NULL); 
    return 0;
}

