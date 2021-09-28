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
#include <poll.h>
#define PORT 4050

using namespace std;

SocketClient::SocketClient(char _hostname[], int _port)
{
	strcpy(m_hostname, _hostname);
	m_port = _port;
	connect_to_server();
};

SocketClient::SocketClient(int _socket)
{
	m_socket = _socket;
};

int SocketClient::send_message(Message _msg)
{
	int n = write(m_socket, &_msg, sizeof(Message));
	if (n < 0)
		printf("ERROR writing to socket");
	cout << "Wrote to socket successfully." << endl;
	return n;
};

Message *SocketClient::receive_message()
{
	Message *message = new Message();
	memset(message, 0, sizeof(Message));
	int n = read(m_socket, message, sizeof(Message));

	if (n < 0)
	{
		// printf("ERROR reading from socket\n");
		return NULL;
	}

	return message;
};

void SocketClient::receive_message_loop()
{
	while (1)
	{
		Message *message = receive_message();
		if (message == NULL)
			continue;
		printf("Got new message from %s in %s with body: %s \n ", message->get_author(), message->get_timestamp_string(), message->get_payload());
	}
};

void SocketClient::send_message_loop()
{
	while (1)
	{
		char payload[180];

		printf("Enter payload: ");
		cin >> payload;
		Message message(Type::UPDATE, 1, 256, payload);
		send_message(message);
		sleep(5);
	}
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
		exit(0);
	}

	if ((m_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("ERROR opening socket\n");
		exit(0);
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(m_port);
	serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
	bzero(&(serv_addr.sin_zero), 8);

	if (connect(m_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("ERROR connecting\n");
		exit(0);
	}

	printf("Connected successfully\n");
	return 0;
}

void SocketClient::close_connection()
{
	close(m_socket);
}

int main(int argc, char *argv[]) // test code
{
	SocketClient mySocket("localhost", PORT);

	struct pollfd pfds[2];

	char buff[256];
	Message *msg = new Message();

	pfds[0].fd = STDIN_FILENO;
	pfds[0].events = POLLIN;

	pfds[1].fd = mySocket.get_socket();
	pfds[1].events = POLLIN;

	uint16_t seq = 0;
	while (1)
	{
		while (poll(pfds, 2, 100 != -1))
		{ /* error handling elided */
			if (pfds[0].revents & POLLIN)
			{
				cout << "Reading data from stdin (I hope) " << endl
					 << flush;
				// read data from stdin and send it over the socket
				cin >> buff;
				Message n(Type::UPDATE, seq++, sizeof(buff), buff);
				send(mySocket.get_socket(), &n, sizeof(Message), NULL); // repeat as necessary.
			}
			if (pfds[1].revents & POLLIN)
			{
				// chat data received
				cout << "Received data!!" << endl
					 << flush;
				recv(mySocket.get_socket(), msg, sizeof(Message), NULL);
				cout << "Data : " << msg->get_payload() << endl
					 << flush;
			}
			if (pfds[1].revents & (POLLERR | POLLHUP))
			{
				// socket was closed
				cout << "oh no" << endl
					 << flush;
			}
		}
	}
	// Message newmsg = Message(Type::ACK, 1, 30, payload);

	// thread reader(&SocketClient::receive_message_loop, mySocket);
	// thread writer(&SocketClient::send_message_loop, mySocket);

	// reader.join();
	// writer.join();

	// mySocket.close_connection();

	return 0;
}
