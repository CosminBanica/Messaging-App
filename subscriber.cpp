#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include "helpers.h"

void usage(char *file) {
    fprintf(stderr, "Usage: %s <ID_Client> <IP_Server> <Port_Server>\n", file);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        usage(argv[0]);
    }

    sub_msg cli_msg;
    tcp_msg *serv_msg;
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[TCPBUFLEN];
    int ret;
    fd_set read_fds, tmp_fds;
    FD_ZERO(&tmp_fds);
    FD_ZERO(&read_fds);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket\n");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[3]));
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton\n");

    ret = connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "connect\n");

    ret = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
    DIE(ret < 0, "send ID\n");

    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    FD_SET(0, &read_fds);
    FD_SET(sockfd, &read_fds);

    while (1) {
        tmp_fds = read_fds;

        ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select\n");

        if (FD_ISSET(0, &tmp_fds)) {
            // comanda de la tastatura
            memset(buffer, 0, UDPBUFLEN);
            fgets(buffer, UDPBUFLEN - 1, stdin);

            // pentru exit clientul devine offline
            if (strncmp(buffer, "exit", 4) == 0) {
                break;
            }

            // se verifica daca e subscribe sau unsubscribe
            if (buffer[0] == 's') {
                cli_msg.tip = 0;
            } else if (buffer[0] == 'u') {
                cli_msg.tip = 1;
            } else {
                printf("can only subscribe or unsubscribe\n");
                continue;
            }

            // se extrage numele topicului
            char *topic_name;
            if (cli_msg.tip == 0) {
                topic_name = strtok(buffer + 10, " ");
            } else if (cli_msg.tip == 1) {
                topic_name = strtok(buffer + 12, "\n");
            }
            
            if (topic_name != nullptr) {
                strcpy(cli_msg.topic, topic_name);
            } else {
                printf("didn't get topic\n");
                continue;
            }

            // pentru subscribe se extrage si SF
            if (cli_msg.tip == 0) {
                topic_name = strtok(nullptr, " ");
                if (topic_name != nullptr) {
                    cli_msg.sf = topic_name[0] - '0';
                } else {
                    printf("Usage: subscribe <topic_name> <SF>\n");
                    continue;
                }
            }

            // se trimite comanda catre server
            ret = send(sockfd, (char*) &cli_msg, sizeof(cli_msg), 0);
            DIE(ret < 0, "send message\n");

            if (cli_msg.tip == 0) {
                printf("subscribed %s\n", cli_msg.topic);
            } else {
                printf("unsubscribed %s\n", cli_msg.topic);
            }

        }
        if (FD_ISSET(sockfd, &tmp_fds)) {
            // mesaj de la un client UDP prin server
            memset(buffer, 0, TCPBUFLEN);
            ret = recv(sockfd, buffer, sizeof(struct tcp_msg), 0);
            DIE(ret < 0, "recv from server\n");
            
            // daca ret == 0, serverul este offline, se inchide si clientul
            if (ret == 0) {
                break;
            }
            serv_msg = (struct tcp_msg*)buffer;
            printf("%s:%d - %s - %s - %s\n", serv_msg->ip, serv_msg->port,
                    serv_msg->topic, serv_msg->tip, serv_msg->data);
        }
    }

    close(sockfd);
    return 0;
}