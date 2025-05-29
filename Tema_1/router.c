#include <arpa/inet.h> /* ntoh, hton and inet_ functions */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib.h"
#include "protocols.h"
#include "queue.h"

FILE *log_file;

typedef struct TrieNode {
    struct TrieNode *children[256];
    int is_end_of_route;
    struct route_table_entry *route_entry;
} TrieNode_rtable;

struct route_table_entry *rtable;
int rtable_len;

/* Mac table */
struct arp_table_entry *arp_table;
int arp_table_len;

TrieNode_rtable *rtable_root = NULL;

// struct route_table_entry *get_best_route(uint32_t ip_dest) {
// 	struct route_table_entry *best = NULL;
// 	for (int i = 0; i < rtable_len; i++) {
// 		if ((rtable[i].prefix & rtable[i].mask) == (ip_dest &
// rtable[i].mask)) { 			if (best == NULL)
// best = &rtable[i]; 			else if (ntohl(best->mask) <
// ntohl(rtable[i].mask)) 				best = &rtable[i];
// 		}
// 	}
// 	return best;
// }

void insert_route(TrieNode_rtable *root, struct route_table_entry entry) {
    // fprintf(log_file, "Mask: %u ", ntohl(entry.mask));
    // fprintf(log_file, "Prefix: %u\n", ntohl(entry.prefix));
    uint32_t mask = ntohl(entry.mask);
    int prefix_length = __builtin_popcount(mask);
    // fprintf(log_file, "Prefix length: %d\n", prefix_length);
    for (int i = 0; i < prefix_length; i++) {
        int bit = (ntohl(entry.prefix) >> (31 - i)) & 1;
        if (root->children[bit] == NULL) {
            root->children[bit] = malloc(sizeof(TrieNode_rtable));
            memset(root->children[bit], 0, sizeof(TrieNode_rtable));
        }
        root = root->children[bit];
    }
    root->is_end_of_route = 1;
    root->route_entry = malloc(sizeof(struct route_table_entry));
    memcpy(root->route_entry, &entry, sizeof(struct route_table_entry));
}

TrieNode_rtable *rtable_to_trie() {
    TrieNode_rtable *root = malloc(sizeof(TrieNode_rtable));
    memset(root, 0, sizeof(TrieNode_rtable));

    for (int i = 0; i < rtable_len; i++) {
        insert_route(root, rtable[i]);
    }

    return root;
}

struct route_table_entry *get_best_route(uint32_t ip_dest) {
    ip_dest = ntohl(ip_dest);
    fprintf(log_file, "Searching for route for %u\n", ntohl(ip_dest));
    TrieNode_rtable *current = rtable_root;
    for (int i = 0; i < 32; i++) {
        int bit = (ip_dest >> (31 - i)) & 1;
        if (current->children[bit] == NULL) {
            break;
        }
        current = current->children[bit];
    }
    if (current->is_end_of_route) {
        // printf("Found route for %u\n", htonl(ip_dest));
        return current->route_entry;
    }
    fprintf(log_file, "No route found for %u\n", ntohl(ip_dest));
    return NULL;
}

struct arp_table_entry *get_arp_table_entry(uint32_t given_ip) {
    for (int i = 0; i < arp_table_len; i++)
        if (arp_table[i].ip == given_ip) return &arp_table[i];
    return NULL;
}

void icmp_send_echo_reply(int type, int interface, char *buf, size_t len,
                          struct ether_hdr *eth_hdr, struct ip_hdr *ip_hdr,
                          struct icmp_hdr *icmp_hdr) {
    size_t data_len = len - sizeof(struct ether_hdr) - sizeof(struct ip_hdr) -
                      sizeof(struct icmp_hdr);
    // Update the icmp header
    icmp_hdr->mtype = 0;
    icmp_hdr->mcode = 0;
    icmp_hdr->check = 0;
    icmp_hdr->check = htons(
        checksum((uint16_t *)icmp_hdr, sizeof(struct icmp_hdr)));
    // Update the ip header
    struct in_addr *temp = malloc(sizeof(struct in_addr));
    inet_aton(get_interface_ip(interface), temp);
    ip_hdr->dest_addr = ip_hdr->source_addr;
    ip_hdr->source_addr = temp->s_addr;
    free(temp);
    ip_hdr->checksum = 0;
    ip_hdr->checksum =
        htons(checksum((uint16_t *)ip_hdr, sizeof(struct ip_hdr)));
    // Update the ethernet header
    memcpy(eth_hdr->ethr_dhost, eth_hdr->ethr_shost, 6);
    get_interface_mac(interface, eth_hdr->ethr_shost);
    send_to_link(len, buf, interface);
}

void icmp_send_dest_unreachable_or_timeout(int type, int interface, char *buf,
                                           size_t len,
                                           struct ether_hdr *eth_hdr,
                                           struct ip_hdr *ip_hdr,
                                           struct icmp_hdr *icmp_hdr) {
    // Update data
    memcpy(icmp_hdr + sizeof(struct icmp_hdr), ip_hdr,
           sizeof(struct ip_hdr) + 8);
    // Update the icmp header
    icmp_hdr->mtype = type;
    icmp_hdr->mcode = 0;
    icmp_hdr->check = 0;
    icmp_hdr->check =
        htons(checksum((uint16_t *)icmp_hdr,
                       sizeof(struct icmp_hdr) + sizeof(struct ip_hdr) + 8));
    // Update the ip header
    struct in_addr *temp = malloc(sizeof(struct in_addr));
    inet_aton(get_interface_ip(interface), temp);
    ip_hdr->dest_addr = ip_hdr->source_addr;
    ip_hdr->source_addr = temp->s_addr;
    free(temp);
    ip_hdr->proto = IPPROTO_ICMP;
    ip_hdr->ttl = 64;
    ip_hdr->tot_len = htons(sizeof(struct ip_hdr) + sizeof(struct icmp_hdr) +
                            sizeof(struct ip_hdr) + 8);
    ip_hdr->checksum = 0;
    ip_hdr->checksum =
        htons(checksum((uint16_t *)ip_hdr, sizeof(struct ip_hdr)));
    // Update the ethernet header
    memcpy(eth_hdr->ethr_dhost, eth_hdr->ethr_shost, 6);
    get_interface_mac(interface, eth_hdr->ethr_shost);
    send_to_link(sizeof(struct ether_hdr) + 2 * sizeof(struct ip_hdr) +
                     sizeof(struct icmp_hdr) + 8,
                 buf, interface);
}

int main(int argc, char *argv[]) {
    log_file = fopen("log.txt", "a+r");
    char buf[MAX_PACKET_LEN];

    // Do not modify this line
    init(argv + 2, argc - 2);

    rtable = malloc(sizeof(struct route_table_entry) * 100000);
    DIE(rtable == NULL, "memory");

    arp_table = malloc(sizeof(struct arp_table_entry) * 100000);
    DIE(arp_table == NULL, "memory");

    rtable_len = read_rtable(argv[1], rtable);
    fprintf(log_file, "Rtable length: %d\n", rtable_len);
    rtable_root = rtable_to_trie();

    queue q = create_queue();
    size_t queue_len = 0;

    while (1) {
        size_t interface;
        size_t len;

        interface = recv_from_any_link(buf, &len);
        DIE(interface < 0, "recv_from_any_links");
        printf("We have received a packet\n");

        struct ether_hdr *eth_hdr = (struct ether_hdr *)buf;

        uint8_t *mac = malloc(6);
        get_interface_mac(interface, mac);

        int destination_found = 0;
        int broadcast_found = 0;
        printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1],
               mac[2], mac[3], mac[4], mac[5]);
        printf("Destination MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
               eth_hdr->ethr_dhost[0], eth_hdr->ethr_dhost[1],
               eth_hdr->ethr_dhost[2], eth_hdr->ethr_dhost[3],
               eth_hdr->ethr_dhost[4], eth_hdr->ethr_dhost[5]);
        printf("Source MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
               eth_hdr->ethr_shost[0], eth_hdr->ethr_shost[1],
               eth_hdr->ethr_shost[2], eth_hdr->ethr_shost[3],
               eth_hdr->ethr_shost[4], eth_hdr->ethr_shost[5]);
        

        for (int i = 0; i < 6; i++) {
            if (eth_hdr->ethr_dhost[i] != 255 && eth_hdr->ethr_dhost[i] != 0) {
                broadcast_found = 1;
                break;
            }
        }
        for (int i = 0; i < 6; i++) {
            if (eth_hdr->ethr_dhost[i] != mac[i]) {
                destination_found = 1;
                break;
            }
        }

        if (destination_found && broadcast_found) {
            continue;
        }
        printf("Packet destination found\n");

        if (ntohs(eth_hdr->ethr_type) == 0x0806) {
            printf("ARP package\n");
            struct arp_hdr *arp_hdr =
                (struct arp_hdr *)(buf + sizeof(struct ether_hdr));
            if (ntohs(arp_hdr->opcode) == 0x0001) {
                printf("Received ARP request\n");
                printf("Dest IP: %u\n", ntohl(arp_hdr->tprotoa));
                // set ethernet header
                memcpy(eth_hdr->ethr_dhost, eth_hdr->ethr_shost,
                       sizeof(eth_hdr->ethr_dhost));
                get_interface_mac(interface, eth_hdr->ethr_shost);
                // set arp header
                arp_hdr->opcode = htons(0x0002);
                memcpy(arp_hdr->thwa, arp_hdr->shwa, sizeof(arp_hdr->thwa));
                arp_hdr->tprotoa = arp_hdr->sprotoa;
                arp_hdr->sprotoa = inet_addr(get_interface_ip(interface));
                memcpy(arp_hdr->thwa, arp_hdr->shwa, sizeof(arp_hdr->thwa));
                get_interface_mac(interface, arp_hdr->shwa);
                printf("Sending ARP reply\n");
                printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       arp_hdr->shwa[0], arp_hdr->shwa[1], arp_hdr->shwa[2],
                       arp_hdr->shwa[3], arp_hdr->shwa[4], arp_hdr->shwa[5]);
                printf("Dest MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       arp_hdr->thwa[0], arp_hdr->thwa[1], arp_hdr->thwa[2],
                       arp_hdr->thwa[3], arp_hdr->thwa[4], arp_hdr->thwa[5]);
                printf("IP: %u\n", ntohl(arp_hdr->sprotoa));
                printf("Dest IP: %u\n", ntohl(arp_hdr->tprotoa));
                // send arp reply
                send_to_link(sizeof(struct ether_hdr) + sizeof(struct arp_hdr),
                             buf, interface);
            }else if (ntohs(arp_hdr->opcode) == 0x0002) {
                printf("Received ARP reply\n");
                struct arp_table_entry *entry =
                    malloc(sizeof(struct arp_table_entry));
                memcpy(entry->mac, arp_hdr->shwa, 6);
                entry->ip = arp_hdr->sprotoa;
                struct arp_table_entry *existing_entry =
                    get_arp_table_entry(entry->ip);
                if (existing_entry != NULL) {
                    memcpy(existing_entry->mac, entry->mac, 6);
                } else {
                    memcpy(arp_table[arp_table_len].mac, entry->mac, 6);
                    arp_table[arp_table_len].ip = entry->ip;
                    arp_table_len++;
                }
                printf("ARP table entry added:\n");
                printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                        entry->mac[0], entry->mac[1], entry->mac[2],
                        entry->mac[3], entry->mac[4], entry->mac[5]);
                printf("IP: %u\n", ntohl(entry->ip));
                
                size_t new_queue_len = queue_len;
                for (int i = 0; i < queue_len; i++) {
                    char *buf = queue_deq(q);
                    struct ether_hdr *eth_hdr = (struct ether_hdr *)buf;
                    struct ip_hdr *ip_hdr =
                        (struct ip_hdr *)(buf + sizeof(struct ether_hdr));
                    struct route_table_entry *best =
                        get_best_route(ip_hdr->dest_addr);
                    printf("Best route found:%u\n", ntohl(best->next_hop));
                    


                    if (best->next_hop != entry->ip) {
                        queue_enq(q, buf);
                        printf("Not for this interface\n");
                        
                        continue;
                    }
                    memcpy(eth_hdr->ethr_dhost, entry->mac,
                           sizeof(eth_hdr->ethr_dhost));
                    get_interface_mac(best->interface, eth_hdr->ethr_shost);
                    eth_hdr->ethr_type = htons(0x0800);
                    printf("Sending packet from queue\n");
                    printf("MAC Destination: %02X:%02X:%02X:%02X:%02X:%02X\n",
                           eth_hdr->ethr_dhost[0], eth_hdr->ethr_dhost[1],
                           eth_hdr->ethr_dhost[2], eth_hdr->ethr_dhost[3],
                           eth_hdr->ethr_dhost[4], eth_hdr->ethr_dhost[5]);
                    send_to_link(len, buf, best->interface);
                    new_queue_len--;
                }
                free(entry);
                queue_len = new_queue_len;
            }

        } else {
            printf("IP package\n");
            struct ip_hdr *ip_hdr =
                (struct ip_hdr *)(buf + sizeof(struct ether_hdr));
            struct icmp_hdr *icmp_hdr =
                (struct icmp_hdr *)(buf + sizeof(struct ether_hdr) +
                                    sizeof(struct ip_hdr));
            struct in_addr *temp = malloc(sizeof(struct in_addr));
            int sent_echo_reply = 0;
            for (int i = 0; i < 3; i++) {
                inet_aton(get_interface_ip(i), temp);
                if (temp->s_addr == ip_hdr->dest_addr) {
                    if (ip_hdr->proto == IPPROTO_ICMP && icmp_hdr->mtype == 8) {
                        printf("Received ICMP echo request\n");
                        icmp_send_echo_reply(0, interface, buf, len, eth_hdr,
                                             ip_hdr, icmp_hdr);
                        printf("Sent ICMP echo reply\n");
                        sent_echo_reply = 1;
                    } else {
                        printf("Ignored non-ICMP packet\n");
                        sent_echo_reply = 1;
                    }
                    continue;
                }
            }
            free(temp);
            if (sent_echo_reply) continue;
            printf("Wanted Ip: %u\n", ntohl(ip_hdr->dest_addr));
            printf("Source Ip: %u\n", ntohl(ip_hdr->source_addr));

            if (checksum((uint16_t *)ip_hdr, sizeof(struct ip_hdr)) != 0)
                continue;
            struct route_table_entry *best = get_best_route(ip_hdr->dest_addr);
            if (best == NULL) {
                icmp_send_dest_unreachable_or_timeout(
                    3, interface, buf, len, eth_hdr, ip_hdr, icmp_hdr);
                continue;
            }
            printf("Best route found:%u\n", htonl(best->next_hop));
            if (ip_hdr->ttl <= 1) {
                icmp_send_dest_unreachable_or_timeout(
                    11, interface, buf, len, eth_hdr, ip_hdr, icmp_hdr);
                continue;
            }
            ip_hdr->ttl--;
            ip_hdr->checksum = 0;
            ip_hdr->checksum =
                htons(checksum((uint16_t *)ip_hdr, sizeof(struct ip_hdr)));

            //Ading the source to ARP table
            struct arp_table_entry *existing_entry =
                get_arp_table_entry(ip_hdr->source_addr);
            if (existing_entry != NULL) {
                memcpy(existing_entry->mac, eth_hdr->ethr_shost, 6);
            } else {
                memcpy(arp_table[arp_table_len].mac, eth_hdr->ethr_shost, 6);
                arp_table[arp_table_len].ip = ip_hdr->source_addr;
                arp_table_len++;
            }

            struct arp_table_entry *mac_address =
                get_arp_table_entry(best->next_hop);
            if (mac_address == NULL) {
                char *copy_buf = malloc(len);
                memcpy(copy_buf, buf, len);
                queue_enq(q, copy_buf);
                queue_len++;
                // printf("Queue length: %zu\n", queue_len);
                // Sending ARP request
                char *buf2 =
                    malloc(sizeof(struct ether_hdr) + sizeof(struct arp_hdr));
                struct ether_hdr *arp_eth_header = (struct ether_hdr *)buf2;
                struct arp_hdr *arp_header =
                    (struct arp_hdr *)(buf2 + sizeof(struct ether_hdr));
                // set ethernet header
                get_interface_mac(best->interface, arp_eth_header->ethr_shost);
                arp_eth_header->ethr_type = htons(0x0806);
                for (int i = 0; i < 6; i++) eth_hdr->ethr_dhost[i] = 255;
                for (int i = 0; i < 6; i++)
                    arp_eth_header->ethr_dhost[i] = 255;
                // set arp header
                arp_header->hw_type = htons(1);
                arp_header->proto_type = htons(0x0800);
                arp_header->hw_len = 6;
                arp_header->proto_len = 4;
                arp_header->opcode = htons(0x0001);
                get_interface_mac(best->interface, arp_header->shwa);
                arp_header->sprotoa =
                    inet_addr(get_interface_ip(best->interface))    ;
                printf("Next hop: %u\n", ntohl(best->next_hop));
                for (int i = 0; i < 6; i++) 
                    arp_header->thwa[i] = 255;
                arp_header->tprotoa = best->next_hop;
                // send arp request
                send_to_link(sizeof(struct ether_hdr) + sizeof(struct arp_hdr),
                             buf2, best->interface);
                printf("Sent ARP request\n");
                printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       arp_header->shwa[0], arp_header->shwa[1],
                       arp_header->shwa[2], arp_header->shwa[3],
                       arp_header->shwa[4], arp_header->shwa[5]);
                printf("Dest MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                       arp_header->thwa[0], arp_header->thwa[1],
                       arp_header->thwa[2], arp_header->thwa[3],
                       arp_header->thwa[4], arp_header->thwa[5]);
                printf("Dest IP: %u\n", ntohl(arp_header->tprotoa));
                printf("IP: %u\n", ntohl(arp_header->sprotoa));
                free(buf2);
                continue;
            }
            memcpy(eth_hdr->ethr_dhost, mac_address->mac,
                   sizeof(eth_hdr->ethr_dhost));
            get_interface_mac(best->interface, eth_hdr->ethr_shost);

            send_to_link(len, buf, best->interface);

        }

        // TODO: Implement the router forwarding logic

        /* Note that packets received are in network order,
                    any header field which has more than 1 byte will need to be
           conerted to host order. For example, ntohs(eth_hdr->ether_type). The
           oposite is needed when sending a packet on the link, */
    }
}
