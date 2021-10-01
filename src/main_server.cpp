//Main includes
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <errno.h>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <iterator>
//Server / Socket-related includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include "data_structures.hpp"
#include <fcntl.h>
//Multi-threading-related includes
#include <thread>
#include <mutex>
#include <condition_variable>
#include <inc/Message.hpp>
#include <iostream>
#include <fstream>
#include <sstream>

//Temp port for now
#define PORT 4001
#define USER_FILE_PATH "listOfUsers.txt"
#define FOLLOWERS_FILE_PATH "listOfUsers.txt"
#define RECEIVES_TWEETS_FILE_PATH "receivedTweets.txt"

std::condition_variable listenerPitstopCV;
std::mutex listenerProceedMUT;

struct userType {
    std::string userName;
    uint32_t userID;
    userType(std::string n, uint32_t id) {
        userName = n;
        userID = id;
    }
};

struct tweetData {
    uint32_t authorID;
    uint64_t tweetID;
    uint64_t timestamp;
    char _payload[256];
    uint32_t numRecipientsRemaining;
};

class customBinarySemaphore {
    std::mutex m;
    int n = 1;
    bool canProceed = false;
    std::condition_variable cv;

    public:
    void P() {
        std::unique_lock<std::mutex> lk(m);
        if (n > 0) {
            n--;
        }
        else{
            cv.wait(lk, [this]{return canProceed;});
            n--;
        }
        canProceed = (n == 0) ? false : true;
        lk.unlock();
    }
    void V() {
        std::unique_lock<std::mutex> lk(m);
        n++;
        canProceed = (n == 0) ? false : true;
        lk.unlock();
        cv.notify_one();
    }
};

struct pendingTweet {
    uint32_t userAuthor;
    uint64_t tweetID; // Timestamp do dado
    pendingTweet(uint32_t author, uint64_t id) {
        userAuthor = author;
        tweetID = id;
    }
};

struct connectionTrackerType {
    uint32_t userID;
    uint8_t numConnections;
    connectionTrackerType(uint32_t id, uint8_t nc) {
        this->userID = id;
        this->numConnections = nc;
    }
};


class connectionManager {
    std::vector<connectionTrackerType> listOfConnectedUsers; //Uses the userID to identify connected users.
        //!  Update this to be a special struct.

    bool registerConnection(uint32_t userID);
    bool closeConnection(uint32_t userID);

    private:
    customBinarySemaphore accessDB;
};

bool connectionManager::registerConnection(uint32_t userID) {

    bool loopCond = true;
    this->accessDB.V();
    std::vector<connectionTrackerType>::iterator itr = this->listOfConnectedUsers.begin();

    while (loopCond) {

        if((*itr).userID == userID) {
            if((*itr).numConnections < 2) {(*itr).numConnections++; this->accessDB.P(); return true;}
            else {this->accessDB.P(); return false;}
        }

        else {
            itr++;
            if(itr == this->listOfConnectedUsers.end()) {
                loopCond = false;
            }
        }
    }

    this->listOfConnectedUsers.push_back(connectionTrackerType(userID, 1));
    this->accessDB.P();
    return true;
}

bool connectionManager::closeConnection(uint32_t userID) {
    bool loopCond = true;
    this->accessDB.V();
    std::vector<connectionTrackerType>::iterator itr = this->listOfConnectedUsers.begin();

    while (loopCond) {

        if((*itr).userID == userID) {
            if((*itr).numConnections == 2) {(*itr).numConnections--; this->accessDB.P(); return true;}
            else {
                int connectionIndex = itr - this->listOfConnectedUsers.begin();
                this->listOfConnectedUsers.erase(this->listOfConnectedUsers.begin() + connectionIndex);
                this->accessDB.P();
                return true;
            }
        }

        else {
            itr++;
            if(itr == this->listOfConnectedUsers.end()) {
                loopCond = false;
            }
        }
    }

    this->accessDB.P();
    return false;
}


class databaseManager {
    std::vector<userType> listOfUsers;;
    std::vector<std::vector<int>> listOfFollowers;
    std::vector<std::vector<tweetData>> listOfReceivedTweets;
    std::vector<std::vector<pendingTweet>> listOfPendingTweets;
    int numUsers = 0;
    //All the above vectors are accessed using semaphores and reader-writer logic.
    //LOU is reader-preferred, LOF is reader preferred, LORT is reader preferred, LOPT is writer-preferred.

    customBinarySemaphore LOU_rw_sem;
    customBinarySemaphore LOF_rw_sem;
    customBinarySemaphore LORT_rw_sem;
    customBinarySemaphore LOPT_rw_sem;
    customBinarySemaphore LOPT_readTry;

    std::mutex LOU_cnt_mutex;
    std::mutex LOF_cnt_mutex;
    std::mutex LORT_cnt_mutex;
    std::mutex LOPT_w_cnt_mut;
    std::mutex LOPT_r_cnt_mut;

    public:
    int saveListOfUsers();
    int loadListOfUsers();
    int loadListOfFollowers();
    int saveListOfFollowers();
    int dataBaseManager::saveListOfReceivedTweets()
    bool addUser(std::string name);
    int getUserIndex(std::string name);
    bool doesClientHavePendingTweets(int userID);
    bool postFollow(std::string targetUserName, int curUserID);
    bool postUpdate(int userID, tweetData tweet);
    std::vector<tweetData> retrieveTweetsFromFollowed(int userID);

    private:
     bool _alreadyFollowed(int targetUserID, int curUserID);
     bool _registerUpdate(int curUserID, tweetData tweet);
     bool _forwardUpdateToFollowers(int curUserID, uint64_t tweetID);
     std::vector<pendingTweet> _retrievePendingTweets(int userID);
     int _getNumFollowers(int curUserID);

     int LOU_cnt = 0;
     int LOF_cnt = 0;
     int LORT_cnt = 0;
     int LOPT_r_cnt = 0;
     int LOPT_w_cnt = 0;
};

//! Incomplete function. Still need to add to all the other vectors.
bool databaseManager::addUser(std::string name) {
    this->LOU_rw_sem.P();

    uint32_t id = this->listOfUsers.size();
    userType u(name, id);
    this->listOfUsers.push_back(u);

    this->listOfFollowers.push_back(std::vector<int>());
    this->listOfReceivedTweets.push_back(std::vector<tweetData>());
    this->listOfPendingTweets.push_back(std::vector<pendingTweet>());

    this->LOU_rw_sem.V();

    //lock_guard automatically unlocks after leaving scope
}

int dataBaseManager::saveListOfReceivedTweets(){

}

int dataBaseManager::loadListOfReceivedTweets(){
    
    std::ifstream inFile;
    inFile.open(RECEIVES_TWEETS_FILE_PATH);


    for (std::string line; std::getline(inFile, line); ) {
        std::cout << line << '\n';
        if(){

        }
        else{
            std::istringstream stream(line);
            uint32_t authorID;
            uint64_t tweetID;
            uint64_t timestamp;
            char _payload[256];
            uint32_t numRecipientsRemaining;
            struct tweetData tweet;

            stream >> authorID >> tweetID >> timestamp >> _payload >> numRecipientsRemaining;
            tweet.authorID = authorID;
        
        

        }
    }
}

int databaseManager::loadListOfFollowers(){
    std::vector<int> row;
    std::ifstream infile(FOLLOWERS_FILE_PATH);
    

    std::string line;
     while (std::getline(infile, line)) {
        std::string digit;

        for (char &c : line) {
            if (c != ' ') {
                digit+=c;
            }
            else if(c==' '){
                row.push_back(stoi(digit));
                digit.clear();
            }
        }

        this->listOfFollowers.push_back(row);
    }

    return 0;
     
}

int databaseManager::saveListOfFollowers(){
    std::ofstream file_obj;
    file_obj.open(FOLLOWERS_FILE_PATH);

    for(std::vector<int> lineOfFollowers : this->listOfFollowers){
        file_obj.write((char*) &lineOfFollowers, sizeof(lineOfFollowers));
    }

    return 0;
}

int databaseManager::saveListOfUsers(){

    std::ofstream file_obj;
    file_obj.open(USER_FILE_PATH);

    for(auto user : this->listOfUsers){
        file_obj.write((char*) &user, sizeof(user) );
    }
    return 0;
}

int databaseManager::loadListOfUsers(){
    std::ifstream file_obj;
    file_obj.open(USER_FILE_PATH,std::ios::in);

    while(!file_obj.eof()){
        userType user("0",0);
        file_obj.read((char*) &user, sizeof(user));
        this->listOfUsers.push_back(user);
    }
    return 0;
}


//! This function has a temporary use. As it stands, the index of the user is synchronized amongst all lists.
//! Ideally, this should return a user ID, which we would use to then search the other lists for the appropriate list corresponding to the user.
//! Currently, it is easier to implement assuming synchronization, and the code, at this stage, is written with that assumption.
int databaseManager::getUserIndex(std::string name) {

    //This function acts as a reader of LOU.
    //Access to LOU is reader-preferred. If it's the first, lock the semaphore.
    std::unique_lock<std::mutex> lk(this->LOU_cnt_mutex);
    this->LOU_cnt++;
    if(this->LOU_cnt == 1) this->LOU_rw_sem.P();
    lk.unlock();

    int n = this->listOfUsers.size();

    bool loopCond = true;
    int i = 0;

    while(i < n && loopCond) {
        if (this->listOfUsers[i].userName == name) loopCond = false;
        else i++;
    }

    //If this is the last reader, free up semaphore.
    lk.lock();
    this->LOU_cnt--;
    if(this->LOU_cnt == 0) this->LOU_rw_sem.V();
    lk.unlock();


        //Return -1 if not found (while loop ran until out of range), else return the indedx
    return (loopCond) ? -1 : i;
}

bool databaseManager::doesClientHavePendingTweets(int userID) {

    //WRiters-preferred read operation
    this->LOPT_readTry.P();
    std::unique_lock<std::mutex> lk_r_m(this->LOPT_r_cnt_mut);
    this->LOPT_r_cnt++;
    if(this->LOPT_r_cnt == 1) this->LOPT_rw_sem.P();
    lk_r_m.unlock();
    this->LOPT_readTry.V();

    bool answer = this->listOfPendingTweets[userID].size() != 0;

    lk_r_m.lock();
    this->LOPT_r_cnt--;
    if(this->LOPT_r_cnt == 0) this->LOPT_rw_sem.V();
    lk_r_m.unlock();

    return answer;
}

bool databaseManager::_alreadyFollowed(int targetUserID, int curUserID) {
    //! Add that one reader/writer check here, otherwise only one thread can check for follows or post follows.

    std::unique_lock<std::mutex> lk(this->LOF_cnt_mutex);
    this->LOF_cnt++;
    if(this->LOF_cnt == 1) this->LOF_rw_sem.P();
    lk.unlock();

    int n = this->listOfFollowers[targetUserID].size();
    int i = 0;
    bool loopCond = true;

    while(i < n && loopCond) {
        if(this->listOfFollowers[targetUserID][i] == curUserID) loopCond = false;
        else i++;
    }

    lk.lock();
    this->LOF_cnt--;
    if(this->LOF_cnt == 0) this->LOF_rw_sem.V();
    lk.unlock();

    return (loopCond) ? false : true;
}

int databaseManager::_getNumFollowers(int curUserID) {

    std::unique_lock<std::mutex> lk(this->LOF_cnt_mutex);
    this->LOF_cnt++;
    if(this->LOF_cnt == 1) this->LOF_rw_sem.P();
    lk.unlock();

    int numberOfFollowers = this->listOfFollowers[curUserID].size();

    lk.lock();
    this->LOF_cnt--;
    if(this->LOF_cnt == 0) this->LOF_rw_sem.V();
    lk.unlock();

    return numberOfFollowers;
}

bool databaseManager::postFollow(std::string targetUserName, int curUserID) {
    int targetUserIndex = this->getUserIndex(targetUserName);
    if(targetUserIndex == -1) {
        std::cout << "WARNING: user " << curUserID << "attempted to follow non-existant user.";
        return false;
    }
    if(this->_alreadyFollowed(targetUserIndex, curUserID) == true) {
        return false;
    }

    this->LOF_rw_sem.P();
    this->listOfFollowers[targetUserIndex].push_back(curUserID);
    this->LOF_rw_sem.V();

    //Check if there are any tweets that have been posted after follow timestamp but before it's finished registering the follow.

    return true;
}

bool databaseManager::_registerUpdate(int curUserID, tweetData tweet) {

    this->LORT_rw_sem.P();
    this->listOfReceivedTweets[curUserID].push_back(tweet);
    this->LORT_rw_sem.V();

    return true;
}

bool databaseManager::_forwardUpdateToFollowers(int curUserID, uint64_t tweetID) {
    //! What happens if someone's FOLLOW command technically happened before the timestamp a tweet was posted, but is only processed after
    //! tweet already went out?

    //! Perhaps a catch-up mechanism in the FOLLOW to add tweet to their pending list.

    int followNum = this->_getNumFollowers(curUserID);

    //Writers-preferred writing access
    std::unique_lock<std::mutex> lk_w_cnt(this->LOPT_w_cnt_mut);
    this->LOPT_w_cnt++;
    if(this->LOPT_w_cnt == 1) this->LOPT_readTry.P();
    lk_w_cnt.unlock();

    this->LOPT_rw_sem.P();
    for (int i = 0; i < followNum; i++) {
        int targetUserID = this->listOfFollowers[curUserID][i];
        this->listOfPendingTweets[targetUserID].push_back(pendingTweet(curUserID, tweetID));
    }
    this->LOPT_rw_sem.V();

    lk_w_cnt.lock();
    this->LOPT_w_cnt--;
    if(this->LOPT_w_cnt == 0) this->LOPT_readTry.V();
    lk_w_cnt.unlock();

    return true;
}

bool databaseManager::postUpdate(int userID, tweetData tweet){

    //Complete the tweet metadata with the number of followers this user has.
    std::unique_lock<std::mutex> lk_u(listenerProceedMUT);
    //Put the tweet in the list of received tweets
    if(!this->_registerUpdate(userID, tweet)) return false;

    //For each user in said list, add pendingTweet regarding current tweet
    if(!this->_forwardUpdateToFollowers(userID, tweet.tweetID)) return false;
        //! weh. Make a function to remove the update if it fails to add it.

    lk_u.unlock();
    listenerPitstopCV.notify_all();



    return true;
}

std::vector<pendingTweet> databaseManager::_retrievePendingTweets(int userID){

    std::vector<pendingTweet> returnList;

    //Writers-preferred read operation
    this->LOPT_readTry.P();
    std::unique_lock<std::mutex> lk_r_m(this->LOPT_r_cnt_mut);
    this->LOPT_r_cnt++;
    if(this->LOPT_r_cnt == 1) this->LOPT_rw_sem.P();
    lk_r_m.unlock();
    this->LOPT_readTry.V();

    returnList = this->listOfPendingTweets[userID];

    lk_r_m.lock();
    this->LOPT_r_cnt--;
    if(this->LOPT_r_cnt == 0) this->LOPT_rw_sem.V();
    lk_r_m.unlock();

    return returnList;
}

std::vector<tweetData> databaseManager::retrieveTweetsFromFollowed(int userID){

    std::vector<tweetData> receivedTweets;
    std::vector<pendingTweet> pendingTweets = this->_retrievePendingTweets(userID);

    std::unique_lock<std::mutex> lk(this->LORT_cnt_mutex);
    this->LORT_cnt++;
    if(this->LORT_cnt == 1) this->LORT_rw_sem.P();
    lk.unlock();

    for (int i = 0; i < pendingTweets.size(); i++) {

        int  j = 0;
        int targetUserID = pendingTweets[i].userAuthor;
        int targetTweet = pendingTweets[i].tweetID;
        int numTargetReceivedTweets = this->listOfReceivedTweets[targetUserID].size();
        while(j < numTargetReceivedTweets) {

            tweetData curTweet = this->listOfReceivedTweets[targetUserID][j];
            if(curTweet.tweetID == targetTweet) {
                receivedTweets.push_back(curTweet);
                break;
            }
            j++;
        }
    }

    lk.lock();
    this->LORT_cnt--;
    if(this->LORT_cnt == 0) this->LORT_rw_sem.V();
    lk.unlock();

    return receivedTweets;

}


databaseManager db_temp;

bool areThereNewTweets = false;
//! NOTE: The above boolean is being used, currently, in place of a check for pending tweets from the database.
//! Any use of it is simply placeholder and not currently functional

void handle_client_listener(bool* connectionShutdownNotice, std::mutex* outgoingQueueMUT,
                            std::vector<Message>* outgoingQueue, bool* outgoingQueueEmpty, int* clientIndex);

void handle_client_speaker(bool* connectionShutdownNotice,
                            std::mutex* incomingQueueMUT, std::vector<Message>* incomingQueue, bool* incomingQueueEmpty,
                            std::mutex* outgoingQueueMUT, std::vector<Message>* outgoingQueue, bool* outgoingQueueEmpty, int* clientIndex);




//NOTE:: Below functions are placeholder and illustrative. It is possible that the handling of services may be shuffled among
//the existing functions, that new functions might be made or existing ones might be removed.

//This function serves to handle the functionality of the Client Connector / Socket manager for each client connection.
//When a connection is made by the Socket Headmaster / Server Connection Controller, this function will be passed onto the new thread.
void handle_client_connector(int socketfd, bool* serverShutdownNotice)
{
    struct pollfd pfd[1];   //Create + set up new polling structure to monitor for incoming messages
    pfd[0].fd = socketfd;
	pfd[0].events = POLLIN;
	fcntl(socketfd, F_SETFL, O_NONBLOCK);   //Set listening socket to be non-blocking so it may work alongside poll
	Message incomingPkt;         //Create pkt structure to be used

	std::vector<Message> outgoingMessages;
	std::vector<Message> incomingMessages;
	std::mutex outgoingMessagesMUT;
	std::mutex incomingMessagesMUT;
    int bytes = 0;
    bool clientShutdownNotice = false;
    bool outgoingQueueEmpty = true;
    bool incomingQueueEmpty = true;    //! Consider if it would not be worth making these into mutices or semaphores.
    int clientIndex = -1;

    std::thread listener(handle_client_listener, &clientShutdownNotice,
                        &outgoingMessagesMUT, &outgoingMessages, &outgoingQueueEmpty, &clientIndex),

                speaker(handle_client_speaker, &clientShutdownNotice,
                            &incomingMessagesMUT, &incomingMessages, &incomingQueueEmpty,
                            &outgoingMessagesMUT, &outgoingMessages, &outgoingQueueEmpty, &clientIndex);

	do {
        int num_events = poll(pfd, 1, 5000);
        std::cout<< "data reading: " << ((num_events > 0) ? "succesfull " : "failed; repeating ") << std::endl << std::flush;
        if(num_events>0)
            std::cout<< incomingPkt.get_payload() << std::endl << std::flush;
//        std::this_thread::sleep_for(5000)
        bytes = recv(socketfd, &incomingPkt, sizeof(incomingPkt), MSG_WAITALL);
        if (bytes == -1)
            printf("TEMP WARNING: NO DATA RECEIVED \n");
        else if (bytes < sizeof(incomingPkt))
            std::cout<< "TEMP WARNING: DATA NOT FULLY READ"<< std::endl;

	} while (*serverShutdownNotice == false || (bytes == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)));
        //! Additionally, check to see if there is anything in the outgoingBuffer

    //If there are messages to be sent to the client, send them first.

    std::unique_lock<std::mutex> lk_om(outgoingMessagesMUT);
    if(outgoingMessages.size() != 0) {
        for (int i = 0; i < outgoingMessages.size(); i++) {

             bytes = write(socketfd, &outgoingMessages[i], sizeof(outgoingMessages[i]));

             if (bytes == -1)
                std::cout << "TEMP WARNING: ERROR SENDING" << std::endl;
            else if (bytes < sizeof(incomingPkt))
                std::cout << "TEMP WARNING: DATA NOT FULLY SENT" << std::endl;
            //! Make it get into a loop here until we are certain the buffer has put all data into buffer
            //! Make it wait for an ack for 20 seconds
                //! Perhaps implement timeout measures for waiting for commands here
            //! Pretty sure we have to wait to check if we can write into the socket beforehand.

        }
        outgoingMessages.clear();
        outgoingQueueEmpty = true;
    }
    lk_om.unlock();

	if(*serverShutdownNotice == true || incomingPkt.get_type() == Type::SHUTDOWN_REQ) {
        //! Check to see how to write using nonblocking sockets. I'm 90% sure you can't just call write
        Message outgoingPkt;
        outgoingPkt.set_type( (*serverShutdownNotice) ? Type::SHUTDOWN_REQ : Type::ACK );
        //! Remember to set the rest of the fields here


        bytes = write(socketfd, &outgoingPkt, sizeof(outgoingPkt));
        //! Make it get into a loop here until we are certain the buffer has put all data into buffer
        //! Make it wait for an ack for 20 seconds
            //! Perhaps implement timeout measures for waiting for commands here

        listener.join();
        speaker.join();

        if(outgoingMessages.size() != 0) {
            for(int i = 0; i < outgoingMessages.size(); i++) {

                //Attempt to send yet unsent messages to client
                //! If we implement 'messages we have attempted to send before' into the database,
                //! this part becomes unnecessary.

                //! Alternatively, we may just accept losing tweets if user signs off just before receiving the ones that
                //! were enqueued.

                //! This is already being done before this. Check things then delete this for loop.

            }
        }
	}
	else if(incomingPkt.get_type() == Type::SIGN_IN) {

        std::cout << "bingus night fever" << std::endl <<std::flush;

        //Check list of connected users for [X]
        //If there is no [X] connected yet,
            //If [X] is in the database, register [X] as connected.
            //Save locally that current user is [X]
        //If [X] is connected to only one other device, mark one more connection.
            //Save locally that current user is [X]
        //If [X] already has two other connections, send rejection message to Client.


	}
	else if(clientIndex == -1) {    //If the pkt isn't sign_in but the user hasn't authenticated yet, refuse message

        Message outgoingPkt;
        outgoingPkt.set_type(Type::NACK);
        std::string message = "User has not authenticated yet.";
        strcpy(outgoingPkt.get_payload(), message.c_str());
        //! Remember to set the rest of the fields here
        //! Perhaps just send error code through Pkt, define meaning of error in header file


        bytes = write(socketfd, &outgoingPkt, sizeof(outgoingPkt));
        //! Make it get into a loop here until we are certain the buffer has put all data into buffer
        //! Make it wait for an ack for 20 seconds
            //! Perhaps implement timeout measures for waiting for commands here

	}
	else if(incomingPkt.get_type() == Type::FOLLOW || incomingPkt.get_type() == Type::UPDATE) {

        //! Yeah might be better to use a mutex for vector access
        //! Perhaps use a semaphore or condition variable to wake up Speaker and Listener?
        std::unique_lock<std::mutex> lk(incomingMessagesMUT);

        incomingMessages.push_back(incomingPkt);
        incomingQueueEmpty = false;

        lk.unlock();
	}

    //create speaker and listener
    //Poll for messages from client
        //! Remember to implement a connection-timeout.
        //! If N seconds pass without request, ping client
    //If there isn't anything new to accept, loop.
    //If there is a shutdown command issued, enter shutdown mode.
        //Request a shutdown from client.
        //Wait for listener and speaker to join.
        //Send whatever updates are still in the outgoing buffer.
        //Close socket.
        //Warn connection list of one less connection of user [X]
        //Finish operation
    //If there is a new message,
        //If it's a shutdown request, enter shutdown mode.
            //Ack the shutdown.
            //Wait for listener and speaker to join
            //Send whatever updates are still in the incoming* buffer.
                //! For fucks sake, refactor the names of listener and speaker.
                //! It is so confusing to see the "listener" send updates and the "speaker" receive the requests.
                //! I get that it's from the perspective of the database, but Blease refactor it
            //Close socket.
            //Warn connection list of one less connection of user [X]
            //Finish operation
        //If user has not signed in and message isn't sign in, reject message
        //If message is Follow, put that info on outgoing buffer, raise speaker flag that there is work to be done.
        //If message is Post Update, put that info on outgoing buffer, raise speaker flag that there is work to be done.
        //If message is Sign In as [X], handle it here.
            //Check list of connected users for [X]
            //If there is no [X] connected yet,
                //If [X] is in the database, mark one more connection.
                //Save locally that current user is [X]
            //If [X] is connected to only one other device, mark one more connection.
                //Save locally that current user is [X]
            //If [X] already has two other connections, send rejection message to Client.

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
void handle_client_listener(bool* connectionShutdownNotice, std::mutex* outgoingQueueMUT,
                            std::vector<Message>* outgoingQueue, bool* outgoingQueueEmpty, int* clientIndex)
{
    bool proceedCondition = false;
    bool stopOperation = false;
    std::unique_lock<std::mutex> lk(listenerProceedMUT);
    while(stopOperation == false){
        while(proceedCondition == false) {
            listenerPitstopCV.wait_for(lk, std::chrono::seconds(10), []{return areThereNewTweets;});
                //! Update this check to use the database check for new tweets.
                //! Alternatively, keep this check but add an aditional one using database.
                //! Remember to update this check to check for tweets we haven't already attempted to send.
                // * Would it make sense to use areThereNewTweets? If the process is woken up forcefully,
                // there will always be new tweets, guaranteed.
            std::cout << "big miss steak " << areThereNewTweets << std::endl << std::flush;
            if (areThereNewTweets == true) proceedCondition = true;
            if (*connectionShutdownNotice == true) {
                proceedCondition = true;
                std::cout << "shutdown ordered; shutting down" << std::endl << std::flush;
                    //! Remove this after implementation ready
            }
        }
        lk.unlock();
            //! Double check the order of lock unlock, whether the unlock should be done here or inside the while.
        if (*connectionShutdownNotice == true) {
            //Currently we just need to exit. There may be need farther down along road of additional operations.
            stopOperation = true;
            continue;
        }

        //Handle the packets that the client has sent
        //Lock the mutex so the connector can't modify it while we use it

        //Grab a copy of the list of incoming tweets for this user using database access
        //For each element in that list, add the corresponding tweet to the temporary vector

        std::vector<tweetData> outTweets = db_temp.retrieveTweetsFromFollowed(*clientIndex);

        std::unique_lock<std::mutex> lk_oq(*outgoingQueueMUT);
            //! Possibly make a local copy of outgoingQueue. Depending how long it takes to write onto database,
            //! might mean the listener holds onto the mutex for less time.
            //! Additionally, the queue shouldn't be too big at any one time.

        for(int i = 0; i < outTweets.size(); i++) {
            Message curPkt;
            curPkt.set_type(Type::UPDATE);
            curPkt.set_timestamp(outTweets[i].timestamp);
            curPkt.set_payload(outTweets[i]._payload);

            (*outgoingQueue).push_back(curPkt);

        }

            //Put the tmeporary vector into the outgoingQueue


        lk_oq.unlock();

        //Update tweets we have attempted to send OR decrement the counter in the tweet thing

    }
    //Access the database, get the list of new tweets.
        //Use author index + tweet ID to retrieve tweets.
            //! Initially, it may be easier to remove items from list of new tweets instead of marking
            //! tweets we have attempted to send but haven't been able to.
        //Add tweets to outgoingQueue
        //Raise flag that there are things in outgoingQueue


    //Deal with shutdown if shutdown was ordered.
    //Else proceed

    //Access the database to get the list of tweets user should receive.
    //Access the database to get the info of the tweets in the above list.
    //Put these tweets in the Outgoing Buffer
    //Access the database to update the tweets we have attempted to send
    //Raise flag that there are tweets to be sent to client
    //Go back to listening for new events



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
void handle_client_speaker(bool* connectionShutdownNotice,
                            std::mutex* incomingQueueMUT, std::vector<Message>* incomingQueue, bool* incomingQueueEmpty,
                            std::mutex* outgoingQueueMUT, std::vector<Message>* outgoingQueue, bool* outgoingQueueEmpty, int* clientIndex)

{

    bool proceedSpeaker = false;
    bool stopOperation = false;
    while (stopOperation == false) {

        while(*incomingQueueEmpty) std::this_thread::sleep_for(std::chrono::seconds(2));

        std::unique_lock<std::mutex> lk(*incomingQueueMUT);

        for(int i = 0; i < incomingQueue->size(); i++ ) {
            Message curPkt = (*incomingQueue)[i];
            if(curPkt.get_type() == Type::FOLLOW) {
                std::string targetUser(curPkt.get_payload());
                //Register client user as following targetUser using database access functions
                bool success = db_temp.postFollow(targetUser, *clientIndex);

                if (success == true) {
                    //
                    Message ackPkt;
                    ackPkt.set_type(Type::ACK);
                    ackPkt.set_timestamp(curPkt.get_timestamp());
                    ackPkt.set_payload(curPkt.get_payload());

                    //Put ACK into outgoing queue.
                    std::unique_lock<std::mutex> lk_t(*outgoingQueueMUT);
                    (*outgoingQueue).push_back(ackPkt);
                    lk_t.unlock();
                }
                else  {
                    Message nackPkt;
                    nackPkt.set_type(Type::ACK);
                    nackPkt.set_timestamp(curPkt.get_timestamp());
                    nackPkt.set_payload(curPkt.get_payload());


                    //Put ACK into outgoing queue.
                    std::lock_guard<std::mutex> lk_t(*outgoingQueueMUT);
                    (*outgoingQueue).push_back(nackPkt);
                }
            }
            else if(curPkt.get_type() == Type::UPDATE) {
                tweetData newTweet;
                newTweet.authorID = *clientIndex;
                newTweet.timestamp = curPkt.get_timestamp();
                strcpy(newTweet._payload, curPkt.get_payload());

                //Register update according to content of curPkt using database access funcitons
                bool success = db_temp.postUpdate(*clientIndex, newTweet);

                if (success == true) {
                    //
                    Message ackPkt;
                    ackPkt.set_type(Type::ACK);
                    ackPkt.set_timestamp(curPkt.get_timestamp());
                    ackPkt.set_payload(curPkt.get_payload());

                    //Put ACK into outgoing queue.
                    std::lock_guard<std::mutex> lk_t(*outgoingQueueMUT);
                    (*outgoingQueue).push_back(ackPkt);
                }
                else  {
                    Message nackPkt;
                    nackPkt.set_type(Type::ACK);
                    nackPkt.set_timestamp(curPkt.get_timestamp());
                    nackPkt.set_payload(curPkt.get_payload());


                    //Put ACK into outgoing queue.
                    std::lock_guard<std::mutex> lk_t(*outgoingQueueMUT);
                    (*outgoingQueue).push_back(nackPkt);
                }

            }
        }


        //Remove everything from incomingQueue
        (*incomingQueue).clear();
        *incomingQueueEmpty = true;
        lk.unlock();

    }




    //For sleep loop until there is something to do
        //! Perhaps use a mutex here? Something like wakeupSpeaker

    //Make a copy of Outgoing buffer, remove items from Outgoing buffer
    //while there is something in that buffer
        //If it's a Follow, use database function to register a follow
        //If it's an Update, use database function to register the tweet
    //Go back on waiting on the for loop


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
void handle_connection_controller(bool* serverShutdownNotice)
{
    int sockfd, newsockfd, n;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	struct pollfd pfd[1];
	std::vector<std::thread> clientConnections;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        printf("ERROR opening socket");

    fcntl(sockfd, F_SETFL, O_NONBLOCK);
	serv_addr.sin_family = AF_INET;
        //! Remember to update this to be over the internet
	serv_addr.sin_port = htons(PORT);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		printf("ERROR on binding");

	listen(sockfd, 15);

	clilen = sizeof(struct sockaddr_in);

	pfd[0].fd = sockfd;
	pfd[0].events = POLLIN;


	do {
        int num_events = poll(pfd, 1, 20000);
        std::cout<< "poll: " << ((num_events > 0) ? "succesful " : "failed; repeating ") << std::endl << std::flush;
        if(num_events > 0) {
            newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
            clientConnections.push_back(std::thread(handle_client_connector, newsockfd, serverShutdownNotice));
                //! Update this to also save the cli_addr and clilen into a vector.
        }
        if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1)
            printf("ERROR on accept");
	} while (*serverShutdownNotice == false);

    for(int i = 0; i < clientConnections.size(); i++) {
        clientConnections[i].join();
    }

    //Creates and sets up the listening socket
    //Uses a non-blocking socket plus poll() to listen for new connections
    //If there isn't anything new to accept, loop.
    //If there is a shutdown notice issued to this thread, go into shutdown mode.
    //If there is a new connection to be accepted, put it to its own thread.
        //! If using a vector to manage the threads, check whether any indexes in the vector are currently non-operational
}

void test_helper_wakeup()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::unique_lock<std::mutex> lk(listenerProceedMUT);
    areThereNewTweets = true;
    listenerPitstopCV.notify_all();
}

void test_helper_shutdownNotice(bool* shutdownNotice)
{
    std::mutex mut;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::unique_lock<std::mutex> lk(mut);
    areThereNewTweets = true;
    listenerPitstopCV.notify_all();
    for (int i =0; i < 100000000000000000; i++ ) i*i*10/26*2;
    lk.unlock();
}

void test_helper_listener(bool* shutdownNotice, std::mutex* mut, int i)
{
    std::cout << "listener " << i << " created " << mut->native_handle() <<  std::endl << std::flush;
    std::unique_lock<std::mutex> lk(*mut);
    for (int i =0; i < 1000000; i++) i*i*10/26*2;
    std::cout << "listener " << i << " has finished" << std::endl << std::flush;
    lk.unlock();

}

void test_helper_speaker(bool* shutdownNotice, std::mutex* mut, int i)
{
    std::cout << "speaker " << i << " created " << mut->native_handle() << std::endl << std::flush;
    std::unique_lock<std::mutex> lk(*mut);
    for (int i =0; i < 1000000000; i++) i*i*10/26*2;
    std::cout << "speaker " << i << " has spoken" << std::endl << std::flush;
    lk.unlock();
    lk.lock();
    for (int i =0; i < 100000000; i++) i*i*10/26*2;
    std::cout << "speaker " << i << " has spoken again" << std::endl << std::flush;
    lk.unlock();

}

void test_helper_connector(bool* serverShutdownNotice, int i)
{
    bool shutdownNotice = false;
    std::mutex mut;
    std::cout << "test: " << mut.native_handle() << std::endl;
    std::thread listener(test_helper_listener, &shutdownNotice, &mut, i),
            speaker(test_helper_speaker, &shutdownNotice, &mut, i);

    listener.join();
    speaker.join();

}



int main(int argc, char **argv)
{
    db_temp.addUser("miku");
    db_temp.addUser("oblige");
    db_temp.addUser("noblesse");

    databaseManager manager;
    manager.addUser("leo");
    manager.addUser("Leah");
    int res = manager.saveListOfUsers();
    // bool shutdownNotice = false;
    // std::thread controller(handle_connection_controller, &shutdownNotice);
    // controller.join();


//     std::mutex testMut;
//     std::cout<< "test: " << testMut.native_handle() << std::endl;
// //    std::thread bingo(handle_client_listener, &shutdownNotice, &testMut), bingo2(test_helper_shutdownNotice, &shutdownNotice);
//     std::thread bingo(test_helper_connector, &shutdownNotice, 1), bingo2(test_helper_connector, &shutdownNotice, 2),
//                 bingo3(test_helper_connector, &shutdownNotice, 3);
//     bingo.join();
//     bingo2.join();
//     bingo3.join();
}
