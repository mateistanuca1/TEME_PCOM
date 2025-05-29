#define MAX_CLIENTS_PER_TOPIC 32
#define MAX_TOPICS 1024
#define MAX_ID_LEN 11
#include "common.h"
#include "payload.h"

typedef struct {
    char topic[64];               // topicul (cheia)
    char subscribers[MAX_CLIENTS_PER_TOPIC][MAX_ID_LEN];  // fd-urile clienților
    int sub_count;
} topic_entry;

#define MAX_ID_LEN 11 // 10 + \0
typedef struct {
    char id[MAX_ID_LEN];  // ID client
    int sockfd;            // FD curent (-1 dacă e deconectat)
    int connected;
    int sent_message; // 0 - nu a trimis mesaj, 1 - a trimis mesaj
} tcp_client;

extern tcp_client clients[32];
extern int num_clients;


extern topic_entry topic_map[MAX_TOPICS];
extern int topic_count;

int find_topic_index(const char *topic);

// Forward declaration of struct chat_packet
void subscribe(const char *topic, const char *client_id);
void unsubscribe(const char *topic, const char *client_id);
void publish(const char *topic, struct send_packet *message, int total_fd);
int find_client_by_id(const char *id);
int find_client_by_fd(int fd);
int add_or_reconnect_client(const char *id, int new_fd);
void handle_disconnect(int sockfd);