/*
 * Protocoale de comunicatii
 * Laborator 7 - TCP
 * Echo Server
 * server.c
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"
#include "payload.h"
#include "utils.h"

#define MAX_CONNECTIONS 32

// Primeste date de pe connfd1 si trimite mesajul receptionat pe connfd2
int receive_and_send(int connfd1, int connfd2, size_t len) {
    int bytes_received;
    char buffer[len];

    // Primim exact len octeti de la connfd1
    bytes_received = recv_all(connfd1, buffer, len);
    // S-a inchis conexiunea
    if (bytes_received == 0) {
        return 0;
    }
    DIE(bytes_received < 0, "recv");

    // Trimitem mesajul catre connfd2
    int rc = send_all(connfd2, buffer, len);
    if (rc <= 0) {
        perror("send_all");
        return -1;
    }

    return bytes_received;
}

void run_chat_multi_server(int tcpfd, int udpfd) {
    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_sockets = 3;
    int rc;

    struct recv_packet received_packet;
    struct udp_payload udp_packet;
    struct sockaddr_in cli_addr;
    socklen_t cli_len = sizeof(cli_addr);

    // Setam socket-ul listenfd pentru ascultare
    rc = listen(tcpfd, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

    // Adaugam noul file descriptor (socketul pe care se asculta conexiuni) in
    // multimea poll_fds
    poll_fds[0].fd = tcpfd;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = udpfd;
    poll_fds[1].events = POLLIN;
    poll_fds[2].fd = STDIN_FILENO;
    poll_fds[2].events = POLLIN;

    while (1) {
        // Asteptam sa primim ceva pe unul dintre cei num_sockets socketi
        rc = poll(poll_fds, num_sockets, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_sockets; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == tcpfd) {
                    // Am primit o cerere de conexiune pe socketul de listen, pe
                    // care o acceptam
                    const int newsockfd =
                        accept(tcpfd, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    int flag = 1;
                    rc = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &flag,
                                    sizeof(int));
                    DIE(rc < 0, "setsockopt TCP_NODELAY");

                    // Adaugam noul socket intors de accept() la multimea
                    // descriptorilor de citire
                    poll_fds[num_sockets].fd = newsockfd;
                    poll_fds[num_sockets].events = POLLIN;
                    num_sockets++;
                } else if (poll_fds[i].fd == udpfd) {
                    // Am primit un mesaj UDP
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    memset(&udp_packet, 0, sizeof(udp_packet));
                    rc = recvfrom(poll_fds[i].fd, &udp_packet,
                                  sizeof(udp_packet), 0,
                                  (struct sockaddr *)&cli_addr, &cli_len);
                    struct send_packet send_packet;
                    send_packet.len = sizeof(send_packet);
                    strcpy(send_packet.payload.topic, udp_packet.topic);
                    send_packet.payload.data_type = udp_packet.data_type;
                    memmove(send_packet.payload.data, udp_packet.data, 1500);
                    send_packet.payload.sender_port = htons(cli_addr.sin_port);
                    strcpy(send_packet.payload.sender_ip,
                           inet_ntoa(cli_addr.sin_addr));
                    publish(udp_packet.topic, &send_packet, num_sockets);

                    DIE(rc < 0, "recvfrom");

                }else if(poll_fds[i].fd == STDIN_FILENO){
                    char buf[100];
                    memset(buf, 0, sizeof(buf));
                    rc = read(STDIN_FILENO, buf, sizeof(buf) - 1);
                    DIE(rc < 0, "read");
                    // Eliminăm newline-ul de la final dacă există
                    buf[strcspn(buf, "\n")] = '\0';
                    if (strlen(buf) == 0) {
                        continue;
                    }
                    if(strcmp(buf, "exit") == 0) {
                        for (int j = 3; j < num_sockets; j++) {
                            close(poll_fds[j].fd);
                        }
                        return;
                    }
                } 
                else {
                    // Am primit date pe unul din socketii de client, asa ca le
                    // receptionam
                    int rc = recv_all(poll_fds[i].fd, &received_packet,
                                      sizeof(received_packet));
                    DIE(rc < 0, "recv");

                    if (rc == 0) {
                        int client_index = find_client_by_fd(poll_fds[i].fd);
                        printf("Client %s disconnected.\n",
                               clients[client_index].id);
                        close(poll_fds[i].fd);

                        // Scoatem din multimea de citire socketul inchis
                        for (int j = i; j < num_sockets - 1; j++) {
                            poll_fds[j] = poll_fds[j + 1];
                        }
                        clients[client_index].sockfd = -1;
                        clients[client_index].connected = 0;
                        num_sockets--;
                    } else {
                        char *command = received_packet.payload.command;
                        if (strcmp(command, "subscribe") == 0) {
                            char *topic = received_packet.payload.topic;
                            int client_index =
                                find_client_by_fd(poll_fds[i].fd);
                            subscribe(topic, clients[client_index].id);
                        } else if (strcmp(command, "unsubscribe") == 0) {
                            char *topic = received_packet.payload.topic;
                            int client_index =
                                find_client_by_fd(poll_fds[i].fd);
                            unsubscribe(topic, clients[client_index].id);
                        } else if (strcmp(command, "connect") == 0) {
                            char *client_id = received_packet.payload.topic;

                            rc = add_or_reconnect_client(client_id,
                                                         poll_fds[i].fd);
                            if (rc == -1) {
                                struct send_packet error_packet;
                                error_packet.len = sizeof(error_packet);
                                error_packet.payload.data_type = 5;
                                send_all(poll_fds[i].fd, &error_packet,
                                         sizeof(error_packet));
                                printf("Client %s already connected.\n",
                                       client_id);
                                close(poll_fds[i].fd);
                                // Scoatem din multimea de citire socketul
                                // inchis
                                for (int j = i; j < num_sockets - 1; j++) {
                                    poll_fds[j] = poll_fds[j + 1];
                                }
                                num_sockets--;
                            } else {
                                char *ip = inet_ntoa(cli_addr.sin_addr);
                                uint16_t port = ntohs(cli_addr.sin_port);
                                printf("New client %s connected from %s:%d.\n",
                                       client_id, ip, port);
                            }
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("\n Usage: %s <port>\n", argv[0]);
        return 1;
    }
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket TCP pentru receptionarea conexiunilor
    const int tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_socket < 0, "socket");
    const int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_socket < 0, "socket");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    // Vezi
    // https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux
    const int enable = 1;
    if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
        0)
        perror("setsockopt(SO_REUSEADDR) failed");
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
        0)
        perror("setsockopt(SO_REUSEADDR) failed");

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // Asociem adresa serverului cu socketul creat folosind bind
    rc = bind(tcp_socket, (const struct sockaddr *)&serv_addr,
              sizeof(serv_addr));
    DIE(rc < 0, "bind");
    rc = bind(udp_socket, (const struct sockaddr *)&serv_addr,
              sizeof(serv_addr));
    DIE(rc < 0, "bind");

    /*
      TODO 2.1: Folositi implementarea cu multiplexare
    */
    // run_chat_server(listenfd);
    run_chat_multi_server(tcp_socket, udp_socket);

    // Inchidem listenfd
    close(tcp_socket);
    close(udp_socket);

    return 0;
}
