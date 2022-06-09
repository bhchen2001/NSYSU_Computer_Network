# ifndef __CLIENT_HPP__
# define __CLIENT_HPP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <queue>
#include <vector>

// network related
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

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

class client_class{
	private:
		int sockfd, seq_num, ack_num, rwnd, port_num, mss, rcv_base;
		struct sockaddr_in servaddr;
        string ip_address;
		vector<string> message_vector;
		queue<struct packet> message_queue;
		vector<pair<int, bool>> client_wnd;
		struct timeval timeout;
		vector<struct packet> packet_buffer;
	public:
		client_class();
		void send_pkt(struct packet);
		struct packet recv_pkt();
        void set_pkt();
		struct packet create_pkt(string, string);
		void* processing(void*);
		int est_connection();
		void set_request(string);
		void write_file(ofstream &fp);
};

#endif