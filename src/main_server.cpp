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
void handle_client_connector(int socket)
{
    //create listener
    //create speaker
    //await for commands from client OR comamnds from listener OR from server manager OR commands from the speaker
        //If the command is from client (Follow, Send Update, Update Ack or Shutdown Notice),
            //If it's a follow, forward that info to the Speaker
            //If it's a Send Update, forward that info to the Speaker
            //### What happens if user sends multiple updates / follow req in sequence? How do we handle that?
            //### How would it handle it by default?
            //!!! What if we make a FIFO list of the commands we've received so far, and then when the Speaker wakes up it acts until
            //!!! the list is empty?
                //This would need a policy of read/write access
                //What would happen if Connector received more requests while the Speaker is dealing with received tasks?
                //!!! Could be remedied by having a stack instead. That way, Speaker removes it from the stack, reads/executes it, then goes on
                //!!! to the next one.
                //!!! Perhaps it'd just be better to have a FIFO that removes it from the list before it executes it.
            // * If it's an Update Ack, forward that info to the Speaker so they may update the corresponding data structures.
                //!!! This is incomplete. What happens if packet isn't received? How does retransmission happen?
                //!!! As long as the Client is connected, it's guaranteed it'll eventually be received.
                //!!! * If client disconnects suddenly, without proper shutdown, what should be done?
                //!!! * If client requests a shutdown, what should be done?
            // If it's a Shutdown Notice, enter shutdown mode (Same instructions as for Server Shutdown)
                // See below for the Server Shutdown instructions
                // Additionally, warn Server Controller that this thread should be joined.
            //Wait for speaker to finish the task it's executing
            //Ack the Client for the task the Speaker finished
                //What could we do to prevent Connector from blocking at the Ack part?
                //!!! Making it so that the Speaker can also warn the Connector
        //If the command is from the Listener, prepare to send updates to client.
            //!!!Client-server protocol dependant; Yet to be decided upon.
        //If the command is from the Speaker, prepare to ack the corresponding command
            //!!! We could make it so that there's a FIFO list of updates to be sent to the client.
            //!!! This would make it so that if a Listener grabs multiple updates and the Speaker acks multiple commands,
            //!!! none of the corresponding packets will be lost and all of them will be waiting in the FIFO list to be
            //!!! sent to the client
            //!!! This way, the Listener or Speaker may awaken the Connector, who may then forward all the commands it
            //!!! receives.
        //If the command is from the server manager,
            //If it's a shutdown notice, enter shutdown mode.
            //Warn the Listener and the Speaker of the shutdown
            //Refuse all further requests from client
            //Close client connection
                //!!! Client-server protocol dependant
            //Wait for Speaker and Connector threads to join this one
            //Finish executing
}

//This funciton handles listening for updates from the people the client follows
//Created by the Client Connector / Socket Manager, one per client connection
void handle_client_listener()
{

    //mutex lock on a shared mutex between listeners
    //condition variable on top of areThereNewTweets

    //Check if there are any updates this user needs upon being created

    //Enter the condition variable to check whether there are tweets this user should receive OR whether thread
    //has entered Shutdown Mode.
        //! Database access protocol dependant; Specifics yet to be decided
        //This check will check whether the List Of Incoming Tweets has new tweets.
            //! On the first run, this is all that will be checked.
            //! The reasoning for this is that, on the wake-up run of the listener, if there should be any
            //! tweets we have attempted to send but haven't been received due to an unexpected shutdown of the
            //! connection between server and client, we may attempt to send them one more time to the client.
            //! The TCP connection guarantees that the data *will* be received once sent by the socket, unless there should
            //! be some unexpected trouble. This covers what should happen to unsent updates if there should occur
            //! such trouble.
        //If it is, nothing to be done, return false.
        //If it isn't, but there are no tweets which we haven't already attempted to send, return false.
        //If there are new tweets we haven't attempted to send, return true.
    //If there are, read / make local copies of all the tweets this user should receive.
        // * If Connector Outgoing Queue not currently being used,
            //Forward this data to the Connector Outgoing Queue
            //Otherwise wait on mutex
        //Warn the Connector that there is data to be sent.
        //Update the List Of Incoming Tweets of the tweets we have attempted to send.
            //! Database access protocol dependant; Specifics yet to be decided
    //If thread has entered Shutdown Mode,
        //! * Potentially mark all tweets in List Of Incoming Tweets as "not attempted to send" to avoid extraneous checks
        //! on the first run when listener is created. Think on this, might not be worth it. Doesn't work if Server has
        //! the a sudden unexpected shutdown
        //Finish operations

}

//This funciton handles posting updates to the server by the user
//Created by the Client Connector / Socket Manager, one per client connection
void handle_client_speaker()
{
    //Speaker locks on Condition Variable (areThereOutgoingUpdates) waiting for outgoing updates from client
    //If woken up, makes a local copy of the buffer
    //If Outgoing Update is a Follow, attempts to register Follow in the database
    //If Outgoing Update is an Update, attempts to register the tweet in the database
        //! Database access protocol dependant; Specifics yet to be decided
        //! Perhaps there could be a global Database Controller object
            //! DB Controller could have a queue, each Speaker inputs orders into this queue
            //! DB Controller then executes these orders in order of queue
            //! * Which thread would execute the orders?
    //If it successfully writes into the database, remove from outgoingBuffer the tweets written into the database.
    //Go back to checking for the condition variable
}

//This function handles listening for connection requests at the server IP and port.
//Created by main, will exist throughout the duration of the server.
void handle_connection_controller()
{
    //Creates and sets up the listening socket
    //Uses a non-blocking socket plus poll() to listen for new connections
}

int main(int argc, char **argv)
{

}
