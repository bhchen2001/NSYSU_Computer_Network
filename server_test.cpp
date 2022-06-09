#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// for poisson distribution
#include <random>
// for multi thread
#include <pthread.h>
#include "server_class.hpp"

using namespace std;
	
// test for server
int main() {

	server_class myServer;
	cout << "server built complete\n";

	myServer.welcome_sock_listen();

	return 0;
}
