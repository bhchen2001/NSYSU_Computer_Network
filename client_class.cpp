# ifndef __CLIENT_CPP__
# define __CLIENT_CPP__

#include "client_class.hpp"
#include <fstream>
#include <algorithm>
// for multi thread
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>


using namespace std;

#define PORT	 	8080
#define BUFFERSIZE 	1024

client_class::client_class(){

	srand(time(NULL));
	seq_num = rand()%10000+1;
	rwnd = 10240;
	ip_address = "127.0.0.1";
	port_num = PORT;
	mss = 1024;
	packet_buffer.reserve(512000/(mss+20));
	struct packet tmppacket;
	tmppacket.myheader.seq_num=-1;
	for(int i=0;i<(512000/(mss+20));i++) packet_buffer.push_back(tmppacket);
	client_wnd.reserve(50);
	for(int i=0;i<50;i++) client_wnd.push_back(make_pair(-1,false));

	cout << "in the constructor...\n";
	for(int p=0;p<2;p++){
		cout << "( " << client_wnd[p].first << ", " << client_wnd[p].second << " ), ";
	}
	cout << endl;

	// creating socket file descriptor
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		perror("socket creation failed");
		exit(EXIT_FAILURE);
	}

	// initialize servaddr
    // servaddr has length of 16 byte
    // sockaddr vs sockaddr_in: https://codertw.com/%E5%89%8D%E7%AB%AF%E9%96%8B%E7%99%BC/392331/
	memset(&servaddr, 0, sizeof(servaddr));

	// setting server information
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(PORT);
	servaddr.sin_addr.s_addr = INADDR_ANY;
}

int client_class::est_connection(){
	// closed :0 --> send syn --> <wait> --> receive syn_ack :1 --> ack_sent --> established --> return est_success 
	int state=0;
	
	while(state<2){
		if(state == 0){
			struct packet syn_packet;
			syn_packet=create_pkt("syn", "");
			send_pkt(syn_packet);
			// wait for syn_ack
			// if(recv_pkt() != -1) state+=1; 
			struct packet recvpacket = recv_pkt();
			ack_num = recvpacket.myheader.seq_num + 1;
			seq_num = recvpacket.myheader.ack_num;
			state+=1;
		}
		else if (state == 1){
			set_pkt();
			// sent and established succeeded
			send_pkt(message_queue.front());
			cout << "sending request with seq_num: " << message_queue.front().myheader.seq_num << ", message: " << message_queue.front().message << endl;
			return 1;
		}
	}
	return 0;
}

void client_class::set_request(string request){
	message_vector.push_back(request);
	return;
}

void client_class::set_pkt(){
	for(int i=0;i<message_vector.size();i++){
		cout << "message_vector: " << message_vector[i] << endl;
		if(!i) message_queue.push(create_pkt("ack", message_vector[i]));
		else message_queue.push(create_pkt("message", message_vector[i]));
		seq_num+=1;
	}
	return;
}

struct packet client_class::create_pkt(string type, string message){
	struct packet mypacket;
	mypacket.myheader.src_port=0;
	mypacket.myheader.des_port=PORT;
	mypacket.myheader.seq_num=seq_num;
	mypacket.myheader.ack_num=ack_num;
	mypacket.myheader.len=0x50; // 01010000
	mypacket.myheader.rwnd = rwnd;
	mypacket.myheader.checksum = 0;
	mypacket.myheader.urgent = 0;

	if(type == "syn") mypacket.myheader.flags = 0x02; 		// 00000010
	else if(type == "ack") mypacket.myheader.flags = 0x00; 	// 00000000
	else if(type == "fin") mypacket.myheader.flags = 0x01; 	// 00000001
	strcpy(mypacket.message, message.c_str());
	return mypacket;
}

void client_class::write_file(ofstream& fp){
	int sending_final = 0;
	while(client_wnd[sending_final+1].second) sending_final++;
	for(int s=0;s<=sending_final;s++){
		char video_buffer[mss];
		for(int v=0;v<mss;v++) video_buffer[v] = packet_buffer[s].message[v];
		fp.write(video_buffer, mss);
	}
	ack_num = client_wnd[sending_final].first + 1;

	// update (shift) client_wnd, packet_buffer, rcv_base
	
	cout << "before update...\n";
	for(int p=0;p<5;p++){
		cout << "( " << client_wnd[p].first << ", " << client_wnd[p].second << " ), ";
	}
	cout << endl;


	rcv_base = (client_wnd[sending_final].first) + 1;
	// cout << "rcv_base: " << rcv_base << ", sending_final: " << sending_final << endl;

	move(client_wnd.begin() + (sending_final + 1), client_wnd.end(), client_wnd.begin());
	fill(client_wnd.begin() + (client_wnd.size() - sending_final - 1), client_wnd.end(), make_pair(-1, false));

	move(packet_buffer.begin() + (sending_final + 1), packet_buffer.end(), packet_buffer.begin());
	struct packet tmppacket;
	tmppacket.myheader.seq_num = -1;
	fill(packet_buffer.begin() + (packet_buffer.size() - sending_final - 1), packet_buffer.end(), tmppacket);

	cout << "after update...\n";
	for(int p=0;p<5;p++){
		cout << "( " << client_wnd[p].first << ", " << client_wnd[p].second << " ), ";
	}
	cout << endl;

	return;

}

void* client_class::processing(void *arg){
	// threeway handshake
	cout << "====Start the three-way handshake=====\n";
	est_connection();
	cout << "====Complete the three-way handshake=====\n\n\n";

	rcv_base = ack_num;
	bool gap =false;

	// send request in message queue
	for(int i=0;i<message_queue.size();i++){
		cout << "waiting for the response: " << i << endl;

		// writing video to the mp4 file
		int video_size = 0, ptr_num = 0;
		struct packet recvpacket;
		ofstream fp("client.mp4", ios::binary);

		cout << "ack_num before receiving packet: " << ack_num << endl;
		recvpacket = recv_pkt();
		while(strcmp(recvpacket.message, "end") != 0){

			// rceive from server nicely
			if(strcmp(recvpacket.message, "error") != 0){
				// in-order packet
				if(recvpacket.myheader.seq_num == ack_num){
					// client_wnd is empty
					if(!client_wnd[0].second && !gap){
						cout << "wait 500ms for the delayed ack\n";
						packet_buffer[recvpacket.myheader.seq_num - ack_num] = recvpacket;
						client_wnd[recvpacket.myheader.seq_num - ack_num] = make_pair(recvpacket.myheader.seq_num, true);
						timeout.tv_sec = 0;
						timeout.tv_usec = 500000;
						setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
						recvpacket = recv_pkt();
						// expected next packet, send delayed ack
						if(recvpacket.myheader.seq_num == ack_num +1){
							cout << "expected packet get, send delayed ack\n";
							packet_buffer[recvpacket.myheader.seq_num - ack_num] = recvpacket;
							client_wnd[recvpacket.myheader.seq_num - ack_num] = make_pair(recvpacket.myheader.seq_num, true);
							write_file(fp);
							send_pkt(create_pkt("ack", ""));
							recvpacket = recv_pkt();
							continue;
						}
						// for the last packet
						else if(recvpacket.myheader.seq_num == client_wnd[0].first){
							cout << "last packet repeated\n";
							write_file(fp);
							send_pkt(create_pkt("ack", ""));
							recvpacket = recv_pkt();
							continue;
						}
						// waiting over timrout, send ack
						else{
							cout << "out-of-order packet\n";
							write_file(fp);
							continue;
						}
					}
					// gap exist and the expected packet arrive
					else if(!client_wnd[0].second && gap){
						cout << "fill the gap: " << recvpacket.myheader.seq_num << endl;
						packet_buffer[recvpacket.myheader.seq_num - ack_num] = recvpacket;
						client_wnd[recvpacket.myheader.seq_num - ack_num] = make_pair(recvpacket.myheader.seq_num, true);
						write_file(fp);
						gap = false;
						for(int c=0;c<client_wnd.size();c++){
							if (client_wnd[c].second) {
								gap = true;
								break;
							}
						}
						send_pkt(create_pkt("ack", ""));
					}

					// cout << "client_wnd: ";
					// for(int p=0;p<2;p++){
					// 	cout << "( " << client_wnd[p].first << ", " << client_wnd[p].second << " ), ";
					// }
					// cout << endl;
				}
				// out-of-order packet
				else if(recvpacket.myheader.seq_num > ack_num){
					cout << "out-of-order packet arrived: " << recvpacket.myheader.seq_num << endl;
					// gap exist
					gap = true;
					// put into buffer if not exceed client_wnd's size
					if((recvpacket.myheader.seq_num - ack_num) < client_wnd.size()){
						packet_buffer[recvpacket.myheader.seq_num - ack_num] = recvpacket;
						client_wnd[recvpacket.myheader.seq_num - ack_num] = make_pair(recvpacket.myheader.seq_num, true);
					}
					// send acc_ack immedaiately
					send_pkt(create_pkt("ack", ""));
				}

				cout << "client_wnd: ";
				for(int p=0;p<5;p++){
					cout << "( " << client_wnd[p].first << ", " << client_wnd[p].second << " ), ";
				}
				cout << endl;
			}
			else if(strcmp(recvpacket.message, "error") == 0){
				if(client_wnd[0].second == true){
					write_file(fp);
					send_pkt(create_pkt("ack", ""));
				}
			}
			// cout << "ptr_num: " << ptr_num++ << endl;
			recvpacket = recv_pkt();
		}


		fp.close();
	}

	send_pkt(create_pkt("fin", ""));

	// close socket
	cout << "client socket closing... " << pthread_self() << endl;
	close(sockfd);

	return NULL;
}

struct packet client_class::recv_pkt(){
	struct packet mypacket;
	socklen_t len = sizeof(servaddr);
	int n = recvfrom(sockfd, (struct header *)&mypacket, sizeof(mypacket), MSG_CONFIRM, (struct sockaddr *) &servaddr, (socklen_t* )&len);
	if (n < 0){
		cout << "fail to recv from server\n";
		return create_pkt("message", "error");
	}
	if(mypacket.myheader.flags == 0x10) cout << "Receive a packet (SYN_ACK) from " << ip_address << mypacket.myheader.src_port << endl;
	cout << "\tRecieve a packet (seq_num = " << mypacket.myheader.seq_num << ", ack_num = " << mypacket.myheader.ack_num << ")\n";
	// seq_num = mypacket.myheader.ack_num;
	return mypacket;
}

void client_class::send_pkt(struct packet mypacket){

	// int sockfd, n;
	// char buffer[BUFFERSIZE];
	// socklen_t len;

	// send packet to server
	if(mypacket.myheader.flags == 0x02) cout << "Send a packet (SYN) to " << ip_address << ": " << port_num << endl;
	else if(mypacket.myheader.flags == 0x00) cout << "Send a packet (ACK) to " << ip_address << ":" << port_num << endl;
	cout << "\tSend a packet (seq_num = " << mypacket.myheader.seq_num << ", ack_num = " << mypacket.myheader.ack_num << ")\n";
	sendto(sockfd, (struct packet *)&mypacket, sizeof(mypacket), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));

	// send message to server
	// sendto(sockfd, (message).c_str(), message.length(), MSG_CONFIRM, (const struct sockaddr *) &servaddr, sizeof(servaddr));
	// cout << "message sent: " << message << endl;



	// receive answer from server

	// char video_buffer[512000];
	// int video_size = 0, ptr_num = 0;

	// while(video_size < 406394){
	// 	n = recvfrom(sockfd, (char *)buffer, BUFFERSIZE, MSG_WAITALL, (struct sockaddr *) &servaddr, (socklen_t *)&len);
	// 	// if(n==0) break;
	// 	for(int i=0;i<n;i++) video_buffer[ptr_num++] = buffer[i];
	// 	// cout << "receive answer from server: " << buffer << endl;
	// 	video_size += 1;
	// 	cout << "current round: " << video_size << endl;
	// }

	// cout << "client get video size: " << video_size << endl;
	// cout << "ptr:" << ptr_num << endl;

	// ofstream fp("client.mp4", ios::binary);
	// fp.write(video_buffer, video_size*1000);
	// fp.close();

	return;
}

# endif