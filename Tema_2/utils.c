#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common.h"

tcp_client clients[32];
int num_clients = 0;

topic_entry topic_map[MAX_TOPICS];
int topic_count = 0;

int find_topic_index(const char *topic) {
    for (int i = 0; i < topic_count; ++i) {
        if (strcmp(topic_map[i].topic, topic) == 0) return i;
    }
    return -1;  // nu există
}

int check_topic_regexp(const char *topic, const char *regexp) {
    if (!strcmp(topic, regexp)) return 1;

    char topic_copy[101], regexp_copy[101];
    strncpy(topic_copy, topic, 100);
    strncpy(regexp_copy, regexp, 100);
    topic_copy[100] = '\0';
    regexp_copy[100] = '\0';

    char *saveptr_topic, *saveptr_regexp;
    char *topic_token = strtok_r(topic_copy, "/", &saveptr_topic);
    char *regexp_token = strtok_r(regexp_copy, "/", &saveptr_regexp);

    while (regexp_token) {
        if (!topic_token && strcmp(regexp_token, "*") != 0) return 0;

        if (strcmp(regexp_token, "*") == 0) {
            // * se potrivește cu orice subtopicuri rămase
            return 1;
        } else if (strcmp(regexp_token, "+") == 0) {
            // match ok, mergem mai departe
        } else if (strcmp(topic_token, regexp_token) != 0) {
            return 0;
        }

        topic_token = strtok_r(NULL, "/", &saveptr_topic);
        regexp_token = strtok_r(NULL, "/", &saveptr_regexp);
    }

    // Dacă topic are în continuare elemente, nu e match complet
    return topic_token == NULL;
}

void subscribe(const char *topic, const char *client_id) {
    int index = find_topic_index(topic);
    if (index == -1) {
        index = topic_count++;
        strncpy(topic_map[index].topic, topic, sizeof(topic_map[index].topic));
        topic_map[index].sub_count = 0;
    }

    // Verificăm dacă ID-ul este deja abonat
    for (int i = 0; i < topic_map[index].sub_count; i++) {
        if (strcmp(topic_map[index].subscribers[i], client_id) == 0) return;
    }

    // Adăugăm ID-ul
    strncpy(topic_map[index].subscribers[topic_map[index].sub_count++],
            client_id, MAX_ID_LEN);
}

void unsubscribe(const char *topic, const char *client_id) {
    int index = find_topic_index(topic);
    if (index == -1) return;

    int count = topic_map[index].sub_count;
    for (int i = 0; i < count; i++) {
        if (strcmp(topic_map[index].subscribers[i], client_id) == 0) {
            // înlocuim cu ultimul și scădem count-ul
            strcpy(topic_map[index].subscribers[i],
                   topic_map[index].subscribers[count - 1]);
            topic_map[index].sub_count--;
            return;
        }
    }
}

void publish(const char *topic, struct send_packet *message, int total_fd) {
    // int index = find_topic_index(topic);
    // if (index == -1) return;

    // for (int i = 0; i < topic_map[index].sub_count; ++i) {
    //     const char *client_id = topic_map[index].subscribers[i];
    //     int client_index = find_client_by_id(client_id);
    //     if (client_index == -1 || !clients[client_index].connected)
    //         continue;

    //     int fd = clients[client_index].sockfd;
    //     send_all(fd, message, sizeof(struct send_packet));
    // }
    for(int i = 0; i < num_clients; i++) {
        clients[i].sent_message = 0;
    }
    for (int i = 0; i < topic_count; ++i) {
        if (check_topic_regexp(topic, topic_map[i].topic)) {
            for (int j = 0; j < topic_map[i].sub_count; ++j) {
                const char *client_id = topic_map[i].subscribers[j];
                int client_index = find_client_by_id(client_id);
                if (client_index == -1 || !clients[client_index].connected)
                    continue;

                int fd = clients[client_index].sockfd;
                if (clients[client_index].sent_message) continue;
                clients[client_index].sent_message = 1;
                send_all(fd, message, sizeof(struct send_packet));
            }
        }
    }
}

int find_client_by_id(const char *id) {
    for (int i = 0; i < num_clients; i++) {
        if (strcmp(clients[i].id, id) == 0) return i;
    }
    return -1;
}

int find_client_by_fd(int fd) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].sockfd == fd) return i;
    }
    return -1;
}

int is_connected_id(const char *id) {
    int i = find_client_by_id(id);
    return i != -1 && clients[i].connected;
}

int add_or_reconnect_client(const char *id, int new_fd) {
    int index = find_client_by_id(id);
    if (index == -1) {
        // client nou
        index = num_clients++;
        strncpy(clients[index].id, id, MAX_ID_LEN);
    }

    if (clients[index].connected) {
        return -1;  // deja conectat
    }

    clients[index].sockfd = new_fd;
    clients[index].connected = 1;
    return index;
}

void handle_disconnect(int sockfd) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].sockfd == sockfd) {
            clients[i].connected = 0;
            clients[i].sockfd = -1;
            printf("Client %s disconnected.\n", clients[i].id);
            break;
        }
    }
}
