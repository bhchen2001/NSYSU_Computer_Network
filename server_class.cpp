# ifndef __SERVER_CPP__
# define __SERVER_CPP__

#include <string.h>
#include <math.h>
#include <fstream>
#include "server_class.hpp"
// for poisson distribution
#include <random>
// for multi thread
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

using namespace std;

typedef void * (*thread_ptr)(void *);

sem_t calc_res;

string calc(char * operation){

    cout << "lock calc_res\n";
    // cout << "operation from connection_socket: " << operation << endl;

    char *substring;
    char *opter;
    
    int operand;
    substring = strtok(operation, " ");
    if(strstr(substring, "sqrt") != NULL){
        cout << "inside the if\n";
        operand = atoi(strtok(NULL, " "));
        operand = sqrt(operand);
        return to_string(operand);
    }
    operand = atoi(substring);

    // cout << "operand:" << substring << endl;
    substring = strtok(NULL, " ");
    strcpy(opter, substring);
    substring = strtok(NULL, " ");
    if (strcmp(opter, "+") == 0 ) operand += atoi(substring);
    else if (strcmp(opter, "-") == 0) operand -= atoi(substring);
    else if (strcmp(opter, "*") == 0) operand *= atoi(substring);
    else if (strcmp(opter, "/") == 0) operand /= atoi(substring);
    else if (strcmp(opter, "**") == 0) operand = pow(operand, atoi(substring));

    cout << "unlock calc_res\n";
    return to_string(operand);
}



server_class::server_class(){

    // creating socket file descriptor 
    if ( (welcome_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("welcome socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 

    // initialize servaddr
    memset(&servaddr, 0, sizeof(servaddr));

    // setting server information
	servaddr.sin_family = AF_INET; // IPv4
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);

    // bind the socket with the server address
	if ( bind(welcome_sockfd, (const struct sockaddr *)&servaddr, 
        sizeof(servaddr)) < 0 ){
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
}

void server_class::welcome_sock_listen(){

    int n, i=0, port = 8081;
    socklen_t len;
    // sem_init(&calc_res, 0, 1);

    pthread_t response_thread[100];
    struct sockaddr_in cliaddr;
    len = sizeof(cliaddr);
    // char buffer[BUFFERSIZE];
    struct packet mypacket;

    while(1){
        srand(time(NULL));
        cout << "listening...\n";
        n = recvfrom(welcome_sockfd, (struct packet *)&mypacket, sizeof(mypacket),
                    MSG_WAITALL, ( struct sockaddr *) &cliaddr,
                    (socklen_t *)&len);
        // buffer[n] = '\0';
        cout << "message from client, seq_num: " << mypacket.myheader.seq_num << endl;

        // create the connection socket arguments
        struct conn_thread_args* conn_args;
        conn_args = (struct conn_thread_args*)malloc(sizeof(struct conn_thread_args));
        conn_args->cliaddr = cliaddr;
        // strcpy(conn_args->message, buffer);
        conn_args->recvpacket = mypacket;
        conn_args->port_num = port++;
        conn_args->seq_num = rand()%10000+1;

        if(pthread_create(&response_thread[i++],
							NULL,
							(thread_ptr)&server_class::connection_sock, // pointer to member function
                            (void *) conn_args                          // connection_socket argument
						    ) != 0)
			cout << "server pthread create failed: " << i << "th\n";

        // free(conn_args);
    }

    i=0;
    if(i<1) pthread_join(response_thread[i++], NULL);
}

struct packet server_class::create_pkt(uint32_t seq_num, uint32_t ack_num, int rwnd, char message[1024], string type){
    struct packet mypacket;
	mypacket.myheader.src_port=0;
	mypacket.myheader.des_port=PORT;
	mypacket.myheader.seq_num=seq_num;
	mypacket.myheader.ack_num=ack_num;
	mypacket.myheader.len=0x50; // 01010000
	mypacket.myheader.rwnd = rwnd;
	mypacket.myheader.checksum = 0;
	mypacket.myheader.urgent = 0;

	if(type == "syn_ack") mypacket.myheader.flags = 0x10; // 00010000
    else if(type == "message"){
        mypacket.myheader.flags = 0x00;
        // strcpy(mypacket.message, message);
        for(int p=0;p<1024;p++){
            mypacket.message[p] = message[p];
        }
    }
	return mypacket;
}

void *server_class::connection_sock(void* args){
    struct sockaddr_in cliaddr = ((struct conn_thread_args*)args)->cliaddr;
    int port = ((struct conn_thread_args*)args)->port_num;
    int ack_num = ((struct conn_thread_args*)args)->recvpacket.myheader.seq_num +1;
    int seq_num = ((struct conn_thread_args*)args)->seq_num;
    int rwnd;
    int cwnd = 100;
    int send_base;
    int mss = 1024;
    char video_buffer[mss];
    socklen_t len = sizeof(cliaddr);

    // initialize the server_wnd
    // three state for the server_wnd: 0 for not sent, 1 for sent & waiting ack
    vector<pair<struct packet, bool>>server_wnd;
    // server_wnd.reserve(cwnd);
    // struct packet tmppacket;
    // tmppacket.myheader.seq_num = -1;
    // fill(server_wnd.begin(), server_wnd.end(), make_pair(tmppacket, false));

    // cout << "get port number: " << ((struct conn_thread_args*)args)->port_num << endl;
    // cout << "seq_num from client: " << ((struct conn_thread_args*)args)->recvpacket.myheader.seq_num << endl;

    // information for connection_socket
    int conn_sockfd;
    struct sockaddr_in connaddr;
    // string answer = calc(message);


    if ( (conn_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("welcome socket creation failed"); 
        exit(EXIT_FAILURE); 
    }

    // initialize connaddr
    memset(&connaddr, 0, sizeof(connaddr));

    // setting connection socket information
	connaddr.sin_family = AF_INET; // IPv4
	connaddr.sin_addr.s_addr = INADDR_ANY;
	connaddr.sin_port = htons(port);

    // cout << "binding port number..." << port << endl;

    // bind the connection socket with the server address
	if ( bind(conn_sockfd, (const struct sockaddr *)&connaddr, 
        sizeof(connaddr)) < 0 ){
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

    // threeway handshake: start from SYN ACK
    int state = 0;

    struct packet mypacket = create_pkt(seq_num, ack_num, rwnd, video_buffer, "syn_ack");
    struct packet recvpacket;
    while(state < 1){
        sendto(conn_sockfd, (const struct packet *) &mypacket, sizeof(mypacket),
                MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                len);
        recvfrom(conn_sockfd, (struct packet*) &recvpacket, sizeof(recvpacket),
                MSG_CONFIRM, (struct sockaddr *) &cliaddr,
                (socklen_t* ) &len);
        seq_num = recvpacket.myheader.ack_num;
        ack_num = recvpacket.myheader.seq_num + 1;
        state+=1;
    }
    cout << "=====Complete the three-way handshake=====\n";
    // threeway handshake complete

    send_base = seq_num;

    do{
        // get request from the recvpacket
        string request = recvpacket.message;
        cout << "get request from client: " << request << endl;

        // start reading the video file...
        ifstream fp(request, ios::binary | ios::ate);

        // cannot open the file
        if(!fp){
            perror("cannot open the file");
            exit(EXIT_FAILURE);
        }

        vector<packet> response_queue;

        int video_size = fp.tellg();
        cout << "video_size: " << video_size << endl;
        fp.seekg(ios::beg);

        while(!fp.eof()){
            fp.read(video_buffer, mss);
            response_queue.push_back(create_pkt(seq_num, ack_num, rwnd, video_buffer, "message"));
            seq_num+=1;
        }

        cout << "all seq_num in the respose_queue: \n";
        for(int p = 0;p < response_queue.size();p++){
            cout << response_queue[p].myheader.seq_num << ", ";
        }
        cout << endl;

        cout << "response_queue create complete\n";

        // put packet into the window
        int last_cwnd_pkt = 0;
        for(;last_cwnd_pkt < cwnd;last_cwnd_pkt++){
            server_wnd.push_back(make_pair(response_queue[last_cwnd_pkt], 0));
        }
        for(int p = 0;p < cwnd;p++){
            cout << server_wnd[p].first.myheader.seq_num << ", ";
        }
        cout << endl;

        while(1){
            // send the packet in the cwnd
            for(int s=0;s<cwnd;s++){
                if(!server_wnd[s].second && server_wnd[s].first.myheader.seq_num != 0){
                    sendto(conn_sockfd, (const struct packet *) &(server_wnd[s].first), sizeof(server_wnd[s].first),
                            MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                            len);
                    server_wnd[s].second = true;
                }
            }
            
            // waiting for the ack of cwnd_base
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 20000;
            setsockopt(conn_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            int n = recvfrom(conn_sockfd, (struct packet*) &recvpacket, sizeof(recvpacket),
                                MSG_CONFIRM, (struct sockaddr *) &cliaddr,
                                (socklen_t* ) &len);
            // timeout for the packet with smallest seq_num
            if(n < 0){
                server_wnd[0].second = false;
                cout << "timeout occurred for send_base: " << send_base << endl;
                continue;
            }
            else{
                int new_send_base = (int)(recvpacket.myheader.ack_num);
                cout << "get new ack_num: " << new_send_base << endl;
                if(new_send_base > response_queue[response_queue.size()-1].myheader.seq_num){
                    cout << "transmition complete\n";
                    break;
                }
                else if((send_base < new_send_base) && (send_base + cwnd > new_send_base)){
                    cout << "acc_ack get: " << new_send_base << endl;
                    cout << "update the server_wnd...\n";
                    int num_to_add = new_send_base - send_base;
                    move(server_wnd.begin() + (num_to_add), server_wnd.end(), server_wnd.begin());
                    for(int a = num_to_add;a>0;a--) server_wnd[cwnd - a] = make_pair(response_queue[last_cwnd_pkt++], false);
                    send_base = new_send_base;
                    cout << "after updating the last_cwnd_pkt, last_cwnd_num: " << last_cwnd_pkt << ",  new_send_base: " << new_send_base << endl;
                    
                    for(int p = 0;p < cwnd;p++){
                        cout << server_wnd[p].first.myheader.seq_num << ", ";
                    }
                    cout << endl;
                }
                else{
                    server_wnd[0].second = false;
                    continue;
                }
            }
        }

        // for(int i=0;i<response_queue.size(); i++){
        //     sendto(conn_sockfd, (const struct packet *) &(response_queue[i]), sizeof(response_queue[i]),
        //             MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
        //             len);
        // }

        cout << "queue sent complete\n";
        strcpy(video_buffer, "end");
        struct packet endpacket = create_pkt(seq_num, ack_num, rwnd, video_buffer, "message");
        sendto(conn_sockfd, (const struct packet *) &(endpacket), sizeof(endpacket),
                    MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
                    len);
        recvfrom(conn_sockfd, (struct packet*) &recvpacket, sizeof(recvpacket),
                    MSG_CONFIRM, (struct sockaddr *) &cliaddr,
                    (socklen_t* ) &len);
    }while(recvpacket.myheader.flags != 0x01);
    
    // close socket
	cout << "Connection socket closing... " << pthread_self() << endl;
	close(conn_sockfd);

    return NULL;
}

// void* server_class::connection_sock(void* args){

//     // information for client
//     char message[BUFFERSIZE];
//     strcpy(message, ((struct conn_thread_args*)args)->message);
//     struct sockaddr_in cliaddr = ((struct conn_thread_args*)args)->cliaddr;
//     int port = ((struct conn_thread_args*)args)->port_num;
//     int len = sizeof(cliaddr);

//     cout << "get port number: " << ((struct conn_thread_args*)args)->port_num << endl;
//     cout << "message from client in connection socket: " << ((struct conn_thread_args*)args)->message << endl;

//     // information for connection_socket
//     int conn_sockfd;
//     struct sockaddr_in connaddr;
//     // string answer = calc(message);


//     if ( (conn_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
//         perror("welcome socket creation failed"); 
//         exit(EXIT_FAILURE); 
//     }

//     // initialize connaddr
//     memset(&connaddr, 0, sizeof(connaddr));

//     // setting connection socket information
// 	connaddr.sin_family = AF_INET; // IPv4
// 	connaddr.sin_addr.s_addr = INADDR_ANY;
// 	connaddr.sin_port = htons(port);

//     // cout << "binding port number..." << port << endl;

//     // bind the connection socket with the server address
// 	if ( bind(conn_sockfd, (const struct sockaddr *)&connaddr, 
//         sizeof(connaddr)) < 0 ){
// 		perror("bind failed");
// 		exit(EXIT_FAILURE);
// 	}

//     // to send the video from server to client depend on the filename

//     ifstream fp(message, ios::binary | ios::ate);

//     // cannot open the file
//     // if(fp == NULL){
//     //     perror("cannot open the file");
//     //     exit(EXIT_FAILURE);
//     // }
//     int mss = 1000, ptr = 0, count = 0;
//     char video_buffer[mss+1];
//     char buf[512000];

//     int video_size = fp.tellg();
//     cout << "video_size: " << video_size << endl;

//     fp.seekg(ios::beg);
//     while(!fp.eof()){
//         fp.read(video_buffer, mss);
//         sendto(conn_sockfd, video_buffer, mss,
//                 MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
//                 len);
//         count+=1;
//     }

//     cout << "server sent times: " << count << endl;

//     fp.close();

//     sendto(conn_sockfd, "", 0,
//             MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
//             len);

//     // sendto(conn_sockfd, answer.c_str(), answer.length(),
//     //         MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
//     //         len);

//     // cout << "server message sent: "  << answer << endl;

//     return NULL;
// }


# endif