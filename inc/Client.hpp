#pragma once
#include <stdint.h>
#include <iostream>
#include <pthread.h>
#include <string>
#include <list>
#include "Message.hpp"

//#include "Socket.hpp"

//
//class ClientSocket : public Socket {
//	public:
//		void connectToServer();
//		void connectToServer(const char* serverAddress, int serverPort);
//};

#define END_OF_INPUT 10
#define FOLLOW_COMMAND "FOLLOW"
#define SEND_COMMAND "SEND"

class Client{
public:

    std::string user;
    int serverPort;
    std::string serverAddress;
    std::vector<Message> outgoingMessages;
	std::vector<Message> incomingMessages;
 //   Socket socket;        TO DO

//need to align these threads with whatever the hell diego is doing in the main file
    Client(std::string user, int serverPort, std::string serverAddress);
    //    static void *controlThread(void* arg); TO DO - CHECK HOW CONTROLLER WILL WORK
    static void *senderThreadExecutor(void* arg);
	static void *receiverThreadExecutor(void* arg);
    static void *displayThreadExecutor(void* arg)


    std::mutex printMUTEX;
    std::mutex controlMUTEX;
    std::mutex inputMUTEX;

 private:
//	void cleanBuffer(void);
    void connectToServer(); // TO DO - needs socket
    void sendTweet(); // TO DO - needs socket
    void sendFollowRequest(); // TO DO - needs socket
    void displayNewFollow(Message *message);
    void displayNewTweet(Message *message);

};


