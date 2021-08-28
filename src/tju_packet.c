#include "tju_packet.h"


/*
 输入header所有字段 和 TCP包数据内容及其长度
 构造tju_packet_t
 返回其指针
 */
tju_packet_t* create_packet(uint16_t src, uint16_t dst, uint32_t seq, 
    uint32_t ack, uint16_t hlen, uint16_t plen, uint8_t flags, 
    uint16_t adv_window, uint8_t ext, char* data, int len) {

    tju_packet_t* new = malloc(sizeof(tju_packet_t));

    new->header.source_port = src;
    new->header.destination_port = dst;
    new->header.seq_num = seq;
    new->header.ack_num = ack;
    new->header.hlen = hlen;
    new->header.plen = plen;
    new->header.flags = flags;
    new->header.advertised_window = adv_window;
    new->header.ext = ext;

    if(len > 0){
        new->data = malloc(len);
        new->data = memcpy(new->data, data, len);
    }else{
        new->data = NULL;
    }
    // 设置checksum
    set_checksum(new);

    return new;
}

/*
 输入header所有字段 和 TCP包数据内容及其长度
 构造tju_packet_t 
 返回其对应的字符串
 */
char* create_packet_buf(uint16_t src, uint16_t dst, uint32_t seq, uint32_t ack,
    uint16_t hlen, uint16_t plen, uint8_t flags, uint16_t adv_window, 
    uint8_t ext, char* data, int len){

    tju_packet_t* temp;
    char* final;  

    temp = create_packet(src, dst, seq, ack, hlen, plen, flags, adv_window, 
        ext, data, len);

    final = packet_to_buf(temp);

    free_packet(temp);
    return final;
}

// 根据接受到的packet构造发送的packet，返回packet的len
int build_state_pkt(char* recv_pkt, char** send_pkt, int flags) {
    int len = HEADER_LEN;
    int seq = get_seq(recv_pkt) + len;
    int ack = get_seq(recv_pkt) + 1;
    *send_pkt = create_packet_buf(
        get_dst(recv_pkt),
        get_src(recv_pkt),
        seq,
        ack,
        len,
        len,
        flags,
        0,
        0,
        NULL,
        0
    );
    return len;
}

/*
 清除一个tju_packet_t的内存占用
 */
void free_packet(tju_packet_t* packet){
    if(packet->data != NULL)
         free(packet->data);
    free(packet);
}

void set_checksum(tju_packet_t* pkt) {
    unsigned short cksum = tcp_compute_checksum(pkt);
    pkt->header.checksum = cksum;
}

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
    if(get_flags(pkt) == ACK) {
        // 发送方收到ACK
        if(get_ack(pkt) == sock->window.wnd_recv->expect_seq) {
            return 0;
        }else {
            return -1;
        }
    }else {
        // 接收方收到发送方发送来的pakcet，此时应当检查发送来的ack是否与expected_seq一致
        // 一致则将其放入缓冲区中，否则丢弃或者存起来
        if(get_seq(pkt) == sock->window.wnd_recv->expect_seq) {
            
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
    // char* data = pkt->data;
    // while(data_len > 0) {
    //     cksum += *data;
    //     data += 1;
    //     data_len -= sizeof(char);
    // }
    cksum = (cksum >> 16) + (cksum & 0xFFFF);
    cksum = ~cksum;
    return (unsigned short)cksum;
}



/*
 下面的函数全部都是从一个packet的字符串中
 根据各个字段的偏移量
 找到并返回对应的字段
*/ 

uint16_t get_src(char* msg){
    int offset = 0;
    uint16_t var;
    memcpy(&var, msg+offset, SIZE16);
    return ntohs(var);
}
uint16_t get_dst(char* msg){
    int offset = 2;
    uint16_t var;
    memcpy(&var, msg+offset, SIZE16);
    return ntohs(var);
}
uint32_t get_seq(char* msg){
    int offset = 4;
    uint32_t var;
    memcpy(&var, msg+offset, SIZE32);
    return ntohl(var);
}
uint32_t get_ack(char* msg){
    int offset = 8;
    uint32_t var;
    memcpy(&var, msg+offset, SIZE32);
    return ntohl(var);
}
uint16_t get_hlen(char* msg){
    int offset = 12;
    uint16_t var;
    memcpy(&var, msg+offset, SIZE16);
    return ntohs(var);
}
uint16_t get_plen(char* msg){
    int offset = 14;
    uint16_t var;
    memcpy(&var, msg+offset, SIZE16);
    return ntohs(var);
}
uint8_t get_flags(char* msg){
    int offset = 16;
    uint8_t var;
    memcpy(&var, msg+offset, SIZE8);
    return var;
}
uint16_t get_advertised_window(char* msg){
    int offset = 17;
    uint16_t var;
    memcpy(&var, msg+offset, SIZE16);
    return ntohs(var);
}
uint8_t get_ext(char* msg){
    int offset = 19;
    uint8_t var;
    memcpy(&var, msg+offset, SIZE8);
    return var;
}

unsigned short get_checksum(char* msg) {
    int offset = 20;
    unsigned short cksum;
    memcpy(&cksum, msg + offset, SIZE16);
    return ntohs(cksum);
}

int is_fin(char* pkt) {
    if(get_flags(pkt) == (ACK | FIN)) {
        return 1;
    }
    return 0;
}



/*############################################## 下面是实现上面函数功能的辅助函数 用户没必要调用 ##############################################*/


/*
 传入header所需的各种数据
 构造并返回header的字符串
 */
char* header_in_char(uint16_t src, uint16_t dst, uint32_t seq, uint32_t ack,
    uint16_t hlen, uint16_t plen, uint8_t flags, uint16_t adv_window, 
    uint8_t ext, unsigned short checksum){

	uint16_t temp16;
    uint32_t temp32;
    
    char* msg = (char*) calloc(plen, sizeof(char));
    int index = 0;
    
    temp16 = htons(src);
    memcpy(msg+index, &temp16, SIZE16);
    index += SIZE16;

    temp16 = htons(dst);
    memcpy(msg+index, &temp16, SIZE16);
    index += SIZE16;

    temp32 = htonl(seq);
    memcpy(msg+index, &temp32, SIZE32);
    index += SIZE32;

    temp32 = htonl(ack);
    memcpy(msg+index, &temp32, SIZE32);
    index += SIZE32;
    
    temp16 = htons(hlen);
    memcpy(msg+index, &temp16, SIZE16);
    index += SIZE16;

    temp16 = htons(plen);
    memcpy(msg+index, &temp16, SIZE16);
    index += SIZE16;

    memcpy(msg+index, &flags, SIZE8);
    index += SIZE8;

    temp16 = htons(adv_window);
    memcpy(msg+index, &temp16, SIZE16);
    index += SIZE16;

    memcpy(msg+index, &ext, SIZE8);
    index += SIZE8;

    temp16 = htons(checksum);
    memcpy(msg + index, &temp16, SIZE16);
    index += SIZE16;

	return msg;
}

// 将buf转换成packet用于计算checksum
tju_packet_t* buf_to_packet(char* buf) {
    tju_packet_t* packet = (tju_packet_t*)malloc(sizeof(tju_packet_t));
    packet->header.source_port = get_src(buf);
    packet->header.destination_port = get_dst(buf);
    packet->header.ack_num = get_ack(buf);
    packet->header.seq_num = get_seq(buf);
    packet->header.hlen = get_hlen(buf);
    packet->header.plen = get_plen(buf);
    packet->header.flags = get_flags(buf);
    packet->header.advertised_window = get_advertised_window(buf);
    packet->header.ext = get_ext(buf);
    packet->header.checksum = get_checksum(buf);

    // 将data从buf拷贝到packet中
    int len = packet->header.plen - packet->header.hlen;
    int offset = packet->header.hlen;
    if (len > 0) {
        packet->data = (char*)malloc(len);
        memcpy(packet->data, buf + offset, len);
    }else {
        packet->data = NULL;
    }

    return packet;
}

/*
 根据传入的tju_packet_t指针
 构造并返回对应的字符串
 */
char* packet_to_buf(tju_packet_t* p){
    char* msg = header_in_char(p->header.source_port, p->header.destination_port, 
        p->header.seq_num, p->header.ack_num, p->header.hlen, p->header.plen, 
        p->header.flags, p->header.advertised_window, 
        p->header.ext, p->header.checksum);
    
    if(p->header.plen > p->header.hlen){
        memcpy(msg+(p->header.hlen), p->data, (p->header.plen - (p->header.hlen)));
    }
        

    return msg;
}
