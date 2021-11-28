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
#include <signal.h>

#define PORT 4050

int m_socket_num;
int m_port;
char m_hostname[280];

using namespace std;

SocketClient::SocketClient(char _hostname[], int _port)
{
	strcpy(m_hostname, _hostname);
	m_port = _port;
	//connect_to_server();
};

SocketClient::SocketClient(int _socket)
{
	m_socket_num = _socket;
};

int SocketClient::send_message(Message _msg)
{
	int error_code;
	send(m_socket_num, &_msg, sizeof(Message), 0);
	unsigned int error_code_size = sizeof(error_code);
	getsockopt(m_socket_num, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);
	return error_code;

};

int SocketClient::send_message_no_retry(Message _msg)
{
	return send(m_socket_num, &_msg, sizeof(Message), 0);
};

Message *SocketClient::receive_message(int *error_code)
{
	Message *message = new Message();
	unsigned int error_code_size = sizeof(error_code);
	memset(message, 0, sizeof(Message));
	int n = read(m_socket_num, message, sizeof(Message));

	if (n <= 0)
	{
		return new Message(Type::DUMMY_MESSAGE, (char*)""); // if couldn't read from server, treat is a dummy message: don't do anything
	}

	getsockopt(m_socket_num, SOL_SOCKET, SO_ERROR, error_code, &error_code_size);

	return message;
};

Message *SocketClient::receive_message_no_retry()
{
	Message *message = new Message();
	memset(message, 0, sizeof(Message));
	int n = read(m_socket_num, message, sizeof(Message));

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

	if ((m_socket_num = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("ERROR opening socket\n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(m_port);
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	if (connect(m_socket_num, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		close(m_socket_num);
		// printf("ERROR connecting\n");
		return -1;
	}

	return 0;
}

void SocketClient::close_connection()
{
	close(m_socket_num);
}