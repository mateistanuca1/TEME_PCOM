/*
 * Protocoale de comunicatii
 * Laborator 7 - TCP si mulplixare
 * client.c
 */

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"
#include "payload.h"
#include "utils.h"
#include <netinet/tcp.h>

void run_client(int sockfd) {
    char buf[MSG_MAXSIZE + 1];
    memset(buf, 0, MSG_MAXSIZE + 1);

    struct recv_packet sent_packet;
    struct send_packet recv_packet;

    struct pollfd fds[2];
    fds[0].fd = sockfd;
    fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    while (1) {
        int rc = poll(fds, 2, -1);
        DIE(rc < 0, "poll");

        // Mesaj de la server
        if (fds[0].revents & POLLIN) {
            rc = recv_all(sockfd, &recv_packet, sizeof(recv_packet));
            if (rc <= 0) {
                printf("Server closed connection.\n");
                break;
            }
            if (recv_packet.payload.data_type == 5) {
                printf("Same ID already connected.\n");
                break;
            }
            recv_packet.payload.data[1499] =
                '\0';  // Asiguram terminarea string-ului
            switch (recv_packet.payload.data_type) {
                case 0: {
                    uint32_t data =
                        *(uint32_t *)((char *)recv_packet.payload.data + 1);
                    uint8_t sign =
                        *(uint8_t *)((char *)recv_packet.payload.data);
                    if (sign == 0) {
                        printf("%s:%d - %s - INT - %u\n",
                               recv_packet.payload.sender_ip,
                               ntohs(recv_packet.payload.sender_port),
                               recv_packet.payload.topic, ntohl(data));
                    } else {
                        printf("%s:%d - %s - INT - -%u\n",
                               recv_packet.payload.sender_ip,
                               ntohs(recv_packet.payload.sender_port),
                               recv_packet.payload.topic, ntohl(data));
                    }
                    break;
                }
                case 1: {
                    uint16_t data =
                        *(uint16_t *)((char *)recv_packet.payload.data);
                    double value = (double)ntohs(data) / (double)100;
                    printf("%s:%d - %s - SHORT_REAL - %.2f\n",
                           recv_packet.payload.sender_ip,
                           ntohs(recv_packet.payload.sender_port),
                           recv_packet.payload.topic, value);
                    break;
                }
                case 2: {
                    uint8_t sign =
                        *(uint8_t *)((char *)recv_packet.payload.data);
                    uint32_t data =
                        *(uint32_t *)((char *)recv_packet.payload.data + 1);
                    uint8_t power_10 =
                        *(uint8_t *)((char *)recv_packet.payload.data + 5);
                    double value = (float)ntohl(data);
                    for (int i = 0; i < power_10; i++) {
                        value /= (float)10;
                    }
                    if (sign == 0) {
                        printf("%s:%d - %s - FLOAT - %.*f\n",
                               recv_packet.payload.sender_ip,
                               ntohs(recv_packet.payload.sender_port),
                               recv_packet.payload.topic, power_10, value);
                    } else {
                        printf("%s:%d - %s - FLOAT - -%.*f\n",
                               recv_packet.payload.sender_ip,
                               ntohs(recv_packet.payload.sender_port),
                               recv_packet.payload.topic, power_10, value);
                    }
                    break;
                }
                case 3: {
                    printf("%s:%d - %s - STRING - %s\n",
                           recv_packet.payload.sender_ip,
                           ntohs(recv_packet.payload.sender_port),
                           recv_packet.payload.topic, recv_packet.payload.data);
                    break;
                }
                default:
                    printf("Unknown data type: %d\n",
                           recv_packet.payload.data_type);
            }
        }

        // Input de la tastatură
        if (fds[1].revents & POLLIN) {
            memset(buf, 0, sizeof(buf));
            fgets(buf, sizeof(buf), stdin);
            DIE(rc < 0, "read");

            // Eliminăm newline-ul de la final dacă există
            buf[strcspn(buf, "\n")] = '\0';

            if (strlen(buf) == 0) {
                continue;
            }
            if (strcmp(buf, "exit") == 0) {
                break;
            }
            if (strncmp(buf, "subscribe", 9) == 0) {
                char *topic = buf + 10;
                memset(&sent_packet, 0, sizeof(sent_packet));
                sent_packet.len = sizeof(sent_packet);
                strcpy(sent_packet.payload.command, "subscribe");
                strcpy(sent_packet.payload.topic, topic);
                printf("Subscribed to topic: %s\n", topic);
                rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
                DIE(rc < 0, "send_all");
            } else if (strncmp(buf, "unsubscribe", 11) == 0) {
                char *topic = buf + 12;
                memset(&sent_packet, 0, sizeof(sent_packet));
                sent_packet.len = sizeof(sent_packet);
                strcpy(sent_packet.payload.command, "unsubscribe");
                strcpy(sent_packet.payload.topic, topic);
                rc = send_all(sockfd, &sent_packet, sizeof(sent_packet));
                DIE(rc < 0, "send_all");
                printf("Unsubscribed from topic: %s\n", topic);
            } else {
                printf("Unknown command: %s\n", buf);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("\n Usage: %s <ID_CLIENT> <IP_SERVER> <PORT_SERVER>\n", argv[0]);
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket TCP pentru conectarea la server
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Ne conectăm la server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    int flag = 1;
    rc = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag,
                    sizeof(int));
    DIE(rc < 0, "setsockopt TCP_NODELAY");

    // Trimitem un mesaj de conectare
    struct recv_packet connect_packet;
    memset(&connect_packet, 0, sizeof(connect_packet));
    connect_packet.len = sizeof(connect_packet);
    strcpy(connect_packet.payload.command, "connect");
    strcpy(connect_packet.payload.topic, argv[1]);  // ID client
    rc = send_all(sockfd, &connect_packet, sizeof(connect_packet));
    DIE(rc < 0, "send_all");

    run_client(sockfd);

    // Inchidem conexiunea si socketul creat
    close(sockfd);

    return 0;
}
