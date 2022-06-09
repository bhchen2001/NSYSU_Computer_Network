#ifndef __SERVER_HPP__
#define __SERVER_HPP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <queue>

// network related
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

#define PORT	 8080
#define BUFFERSIZE 1024

struct header{
	uint16_t src_port;
	uint16_t des_port;
	uint32_t seq_num;
	uint32_t ack_num;
	uint8_t len;
	uint8_t flags;
	uint16_t rwnd;
	uint16_t checksum;
	uint16_t urgent; 
};

struct packet{
	struct header myheader;
	char message[1024];
};

struct conn_thread_args{
    struct sockaddr_in cliaddr;
    char message[BUFFERSIZE];
    int port_num;
    struct packet recvpacket;
    int seq_num;
};

class server_class{
    private:
        struct sockaddr_in servaddr;
        int welcome_sockfd;
    public:
        server_class();
        void welcome_sock_listen();
        static void* connection_sock(void*);
        static struct packet create_pkt(uint32_t, uint32_t, int, char*, string);
};

#endif