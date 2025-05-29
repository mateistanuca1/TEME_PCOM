#ifndef __PAYLOAD_H__
#define __PAYLOAD_H__
#include <stddef.h>
#include <stdint.h>

#define MSG_MAXSIZE 2000

struct __attribute__((__packed__)) udp_payload {
    char topic[50];
    uint8_t data_type;
    char data[1500];
};

struct __attribute__((__packed__)) tcp_send_payload {
    char sender_ip[16];
    uint16_t sender_port;
    char topic[50];
    uint8_t data_type; // 5 - error 6 - subscribe 7 - unsubscribe
    char data[1500];
};

struct __attribute__((__packed__)) send_packet {
    uint16_t len;
    struct tcp_send_payload payload;
};

struct __attribute__((__packed__)) tcp_recv_payload {
    char command[50];
    char topic[50];  // optional, doar daca e subscribe / unsubscribe, altfel il
                     // folosesc ca id
};

struct __attribute__((__packed__)) recv_packet {
    uint16_t len;
    struct tcp_recv_payload payload;
};


#endif