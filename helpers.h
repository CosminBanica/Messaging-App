#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <cstdlib>
#include <cstdio>
#include <cstdint>

// mesajul pe care il va primi serverul de la un client udp;
struct __attribute__((packed)) sub_msg {
    uint8_t tip;
    char topic[51];
    bool sf;
};

// mesajul pe care il va primi clientul TCP
struct __attribute__((packed)) tcp_msg {
    char ip[16];
    uint16_t port;
    char topic[51];
    char tip[11];
    char data[1501];
};

// mesajul pe care il va primi serverul de la un client UDP
struct __attribute__((packed)) udp_msg {
    char topic[50];
    uint8_t tip_date;
    char data[1501];
};

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define TCPBUFLEN		(sizeof(tcp_msg) + 1)
#define UDPBUFLEN		(sizeof(udp_msg) + 1)

#endif