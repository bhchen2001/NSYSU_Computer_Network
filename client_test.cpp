#include <string.h>
#include <time.h>
// for multi thread
#include <pthread.h>
#include "client_class.hpp"
typedef void * (*thread_ptr)(void *);

using namespace std;

// poisson distribution
// std::default_random_engine generator;
// std::poisson_distribution<int> distribution(4.1);

// test for client
int main(int argc, char *argv[]) {

	srand(time(NULL));

	int client_num = 1;
	string testbench[6]={"5 + 2", "5 - 2", "5 * 2", "5 / 2", "5 ** 2", "6 ** 2"};
	string testbench_video[7] = {"pic/Lenna.jpg", "vid/1.mp4", "vid/2.mp4", "vid/3.mp4", "vid/4.mp4", "vid/5.mp4", "vid/6.mp4"};

	client_class* myClient[client_num];

	for(int i=0;i<client_num;i++)	(myClient[i]) = new client_class();
	pthread_t request_thread[100];
	cout << "client build complete\n";

	// start sending message
	for(int i=0;i<client_num;i++){
		// set the sending packet
		// myClient[i]->set_pkt(testbench[(rand()%6)]);
		myClient[i]->set_request(testbench_video[1]);
		if(pthread_create(&request_thread[i],
							NULL,
							(thread_ptr)&client_class::processing, 	// pointer to member function
							myClient[i] 							//the object
							) != 0)
			cout << "pthread create failed: " << i << "th\n";
	}

	int i = 0;
	while(i < client_num) pthread_join(request_thread[i++],NULL);
	
	return 0;
}
