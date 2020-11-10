#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <cmath>
#include "helpers.h"

using namespace std;

// permite folosirea unui set de pairs
struct pair_hash { 
    template <class T1, class T2> 
    size_t operator()(const pair<T1, T2>& p) const { 
        auto hash1 = hash<T1>{}(p.first); 
        auto hash2 = hash<T2>{}(p.second); 
        return hash1 ^ hash2; 
    } 
}; 

void usage(char *file)
{
	fprintf(stderr, "Usage: %s <PORT_DORIT>\n", file);
	exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
		usage(argv[0]);
	}

    int tcp_sockfd, udp_sockfd, new_sockfd, portno;
	char buffer[UDPBUFLEN];
	struct sockaddr_in udp_serv_addr, tcp_serv_addr, cli_addr;
	socklen_t tcp_clilen, udp_clilen;
    int fdmax, ret;
    fd_set read_fds, tmp_fds;
    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

    portno = atoi(argv[1]);
	DIE(portno == 0, "atoi\n");

    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "TCP socket\n");

    udp_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "UDP socket\n");

    memset((char *) &tcp_serv_addr, 0, sizeof(tcp_serv_addr));
	tcp_serv_addr.sin_family = AF_INET;
	tcp_serv_addr.sin_port = htons(portno);
	tcp_serv_addr.sin_addr.s_addr = INADDR_ANY;

    memset((char *) &udp_serv_addr, 0, sizeof(udp_serv_addr));
	udp_serv_addr.sin_family = AF_INET;
	udp_serv_addr.sin_port = htons(portno);
	udp_serv_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(tcp_sockfd, (struct sockaddr *) &tcp_serv_addr,
        sizeof(struct sockaddr_in));
	DIE(ret < 0, "TCP bind\n");

    ret = bind(udp_sockfd, (struct sockaddr *) &udp_serv_addr,
        sizeof(struct sockaddr_in));
	DIE(ret < 0, "UDP bind\n");

	ret = listen(tcp_sockfd, 1000000);
	DIE(ret < 0, "TCP listen\n");

    // clientii offline cu SF == 1 salveaza mesajele de primit la revenire
    unordered_map<string, vector<tcp_msg>> offline_clients;
    // id-ul si SF-ul clientilor abonati la un topic
    unordered_map<string, unordered_set<pair<string, bool>,
        pair_hash>> subscribed_clients;
    // fd asociat id-ului unui client
    unordered_map<string, int> asoc_fds;
    // id asociat fd-ului unui client
    unordered_map<int, string> asoc_ids;
    // pt id-ul unui client, daca e true, e online, altfel e offline
    unordered_map<string, bool> online_clients;


    FD_SET(0, &read_fds);
    FD_SET(tcp_sockfd, &read_fds);
    FD_SET(udp_sockfd, &read_fds);
    if (udp_sockfd > tcp_sockfd) {
        fdmax = udp_sockfd;
    } else {
        fdmax = tcp_sockfd;
    }

    bool active = true;
    while (active) {
        tmp_fds = read_fds;

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select\n");

        for (int i = 0; i <= fdmax; ++i) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == tcp_sockfd) {
                    // nou client TCP
                    new_sockfd = accept(tcp_sockfd, (sockaddr*) &cli_addr,
                            &tcp_clilen);
                    DIE(new_sockfd < 0, "accept\n");

                    FD_SET(new_sockfd, &read_fds);

                    if (new_sockfd > fdmax) {
                        fdmax = new_sockfd;
                    }

                    // se primeste id-ul clientului
                    memset(buffer, 0, TCPBUFLEN);
                    ret = recv(new_sockfd, buffer, TCPBUFLEN - 1, 0);
                    DIE(ret < 0, "recv new client\n");

                    char id[11];
                    strcpy(id, buffer);

                    // clientul primeste mesajele pentru SF == 1
                    for (auto buf_msg : offline_clients[id]) {
                        ret = send(new_sockfd, (char*) &buf_msg, sizeof(tcp_msg), 0);
                        DIE(ret < 0, "send offline messages to client\n");
                    }

                    online_clients[id] = true;
                    if (!offline_clients[id].empty()) {
                        offline_clients[id].clear();
                    }
                    // se updateaza legatura intre fd si id ale clientului
                    asoc_fds[id] = new_sockfd;
                    asoc_ids[new_sockfd] = id;

                    printf("New client %s connected from %s:%d.\n", id,
                        inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                    
                    int flag = 1;
                    setsockopt(new_sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag,
                            sizeof(int));
                } else if (i == udp_sockfd) {
                    // mesaj de la client UDP
                    memset(buffer, 0, UDPBUFLEN);
                    ret = recvfrom(udp_sockfd, buffer, UDPBUFLEN, 0,
                        (struct sockaddr *) &udp_serv_addr, &udp_clilen);
                    DIE(ret < 0, "recv UDP\n");
                    
                    // se pun informatiile relevante pentru subscriber in new_msg
                    tcp_msg new_msg;
                    udp_msg *rec_msg; 
                    strcpy(new_msg.ip, inet_ntoa(udp_serv_addr.sin_addr));
                    new_msg.port = ntohs(udp_serv_addr.sin_port);
                    rec_msg = (struct udp_msg*) buffer;

                    strncpy(new_msg.topic, rec_msg->topic, 50);
                    new_msg.topic[50] = 0;

                    if (rec_msg->tip_date == 0) {
                        strcpy(new_msg.tip, "INT");
                        int n = ntohl(*(uint32_t*)(rec_msg->data + 1));

                        if (rec_msg->data[0] == 1) {
                            n *= -1;
                        }
                        sprintf(new_msg.data, "%d", n);
                    }

                    if (rec_msg->tip_date == 1) {
                        strcpy(new_msg.tip, "SHORT_REAL");
                        double n;
                        n = ntohs(*(uint16_t*)(rec_msg->data)) / 100;
                        sprintf(new_msg.data, "%.2f", n);
                    }

                    if (rec_msg->tip_date == 2) {
                        strcpy(new_msg.tip, "FLOAT");
                        double n;
                        n = ntohl(*(uint32_t*)(rec_msg->data + 1)) /
                            pow(10, rec_msg->data[5]);

                        if (rec_msg->data[0] == 1) {
                            n *= -1;
                        }
                        sprintf(new_msg.data, "%lf", n);
                    }

                    if (rec_msg->tip_date == 3) {
                        strcpy(new_msg.tip, "STRING");
                        strcpy(new_msg.data, rec_msg->data);
                    }

                    for (auto id : subscribed_clients[new_msg.topic]) {
                        if (online_clients[id.first]) {
                            // daca subscriberul e online i se trimite mesajul
                            ret = send(asoc_fds[id.first], (char*) &new_msg,
                                sizeof(tcp_msg), 0);
                            DIE(ret < 0, "send to TCP from UDP\n");
                        } else if (id.second == 1) {
                            // daca subscriberul e offline si are SF == 1
                            // se salveaza mesajul
                            offline_clients[id.first].push_back(new_msg);
                        }
                    }
                } else if (i == 0) {
                    // comanda de la stdin
                    memset(buffer, 0, UDPBUFLEN);
                    fgets(buffer, UDPBUFLEN - 1, stdin);

                    if (strncmp(buffer, "exit", 4) == 0) {
                        active  = false;
                        break;
                    } else {
                        printf("unknown command\n");
                    }
                } else {
                    // comanda de la client TCP
                    memset(buffer, 0, UDPBUFLEN);
                    ret = recv(i, buffer, UDPBUFLEN - 1, 0);
                    DIE(ret < 0, "recv command from client\n");

                    if (ret == 0) {
                        string ID = asoc_ids[i];
                        char id[11];
                        for (int j = 0; j < ID.size(); ++j) {
                            id[j] = ID[j];
                        }
                        id[ID.size()] = 0;
                        printf("Client %s disconnected.\n", id);
                        FD_CLR(i, &read_fds);

                        // clientul devine offline si se sterge legatura
                        // intre fd si id ale acestuia
                        online_clients[asoc_ids[i]] = false;
                        asoc_fds.erase(asoc_ids[i]);
                        asoc_ids.erase(i);
                        close(i);
                    } else {
                        sub_msg *rec_msg = (sub_msg*)buffer;
                        if (rec_msg->tip == 0) {
                            // se face subscribe la topic
                            // mai intai se sterge vechiul subscribe la topic
                            subscribed_clients[rec_msg->topic].erase(
                                    make_pair(asoc_ids[i], 0));
                            subscribed_clients[rec_msg->topic].erase(
                                    make_pair(asoc_ids[i], 1));

                            subscribed_clients[rec_msg->topic].insert(
                                    make_pair(asoc_ids[i], rec_msg->sf));
                        } else {
                            // se face unsubscribe la topic
                            subscribed_clients[rec_msg->topic].erase(
                                    make_pair(asoc_ids[i], 0));
                            subscribed_clients[rec_msg->topic].erase(
                                    make_pair(asoc_ids[i], 1));
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i <= fdmax; ++i) {
        if (FD_ISSET(i, &read_fds)) {
            close(i);
        }
    }

    return 0;
}