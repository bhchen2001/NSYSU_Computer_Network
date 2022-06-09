CC = g++
CLIENT_TEST_FILE = client_test
SERVER_TEST_FILE = server_test
CLIENT_CLASS_FILE = client_class
SERVER_CLASS_FILE = server_class

CLIENT_OUT = client
SERVER_OUT = server

all:
	clear
	$(CC) -c $(CLIENT_CLASS_FILE).cpp $(SERVER_CLASS_FILE).cpp $(CLIENT_TEST_FILE).cpp $(SERVER_TEST_FILE).cpp
	$(CC) -o $(CLIENT_OUT).out $(CLIENT_TEST_FILE).o $(CLIENT_CLASS_FILE).o -lpthread
	$(CC) -o $(SERVER_OUT).out $(SERVER_TEST_FILE).o $(SERVER_CLASS_FILE).o -lpthread

clean : 
	rm *.o $(CLIENT_OUT).out $(SERVER_OUT).out *.mp4