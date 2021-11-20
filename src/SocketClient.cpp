#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <iostream>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <inc/Message.hpp>
#include <inc/Socket.hpp>
#include <thread>
#include <condition_variable>
#include <poll.h>
#define PORT 4050

using namespace std;

extern std::mutex frontEndMutex;
extern std::condition_variable frontEndCondVar;
extern bool lookForServer;
extern SocketClient m_socket;

SocketClient::SocketClient(char _hostname[], int _port)
{
	strcpy(m_hostname, _hostname);
	m_port = _port;
	//connect_to_server();
};

SocketClient::SocketClient(int _socket)
{
	m_socket = _socket;
};

int SocketClient::send_message(Message _msg)
{
	int n = write(m_socket, &_msg, sizeof(Message)) == -1;
	cout << "n is: " << n << "errno is: " << errno << endl << flush;
	while (n != 0)
	{
		{
			lookForServer = true;
			std::unique_lock<std::mutex> lock(frontEndMutex);
			frontEndCondVar.wait_for(lock, std::chrono::seconds(1000), []()
									 { return !lookForServer; });
		}
		n = send(m_socket, &_msg, sizeof(Message), NULL) == -1;
	}
	return 0;
};

int SocketClient::send_message_no_retry(Message _msg)
{
	return send(m_socket, &_msg, sizeof(Message), NULL);
};

Message *SocketClient::receive_message()
{
	Message *message = new Message();
	memset(message, 0, sizeof(Message));
	int n = read(m_socket, message, sizeof(Message));

	if (n <= 0)
	{
		return new Message(Type::DUMMY_MESSAGE, ""); // if couldn't read from server, treat is a dummy message: don't do anything
	}

	{ // reestablish server connnection
		lookForServer = true;
		std::unique_lock<std::mutex> lock(frontEndMutex);
		frontEndCondVar.wait_for(lock, std::chrono::seconds(1000), []()
								 { return !lookForServer; });
	}

	return message;
};

Message *SocketClient::receive_message_no_retry()
{
	Message *message = new Message();
	memset(message, 0, sizeof(Message));
	int n = read(m_socket, message, sizeof(Message));

	return message;
};

int SocketClient::connect_to_server()
{
	int n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	server = gethostbyname(m_hostname);
	if (server == NULL)
	{
		fprintf(stderr, "ERROR, no such host\n");
		return -1;
	}

	if ((m_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("ERROR opening socket\n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(m_port);
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	if (connect(m_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("ERROR connecting\n");
		return -1;
	}

	return 0;
}

void SocketClient::close_connection()
{
	close(m_socket);
}
