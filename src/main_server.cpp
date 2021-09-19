//Main includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <errno.h>
//Server / Socket-related includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <data_structures.hpp>
//Multi-threading-related includes
#include <thread>
#include <mutex>
#include <condition_variable>
//Temp port for now
#define PORT 4000

//Current headers are placeholder; It is possible that some of them are unnecessary.




//NOTE:: Below functions are placeholder and illustrative. It is possible that the handling of services may be shuffled among
//the existing functions, that new functions might be made or existing ones might be removed.

//This function serves to handle the functionality of the Client Connector / Socket manager for each client connection.
//When a connection is made by the Socket Headmaster / Server Connection Controller, this function will be passed onto the new thread.
void handle_client_connector()
{
}

//This funciton handles listening for updates from the people the client follows
//Created by the Client Connector / Socket Manager, one per client connection
void handle_client_listener()
{
}

//This funciton handles posting updates to the server by the user
//Created by the Client Connector / Socket Manager, one per client connection
void handle_client_speaker()
{
}

//This function handles listening for connection requests at the server IP and port.
//Created by main, will exist throughout the duration of the server.
void handle_connection_controller()
{
}

int main(int argc, char **argv)
{

}
