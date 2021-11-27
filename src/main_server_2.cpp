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
#include <iostream>
//Server / Socket-related includes
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include "data_structures.hpp"
#include <fcntl.h>
//Multi-threading-related includes
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <fstream>
#include <sstream>
#include <inc/ElectionManager.hpp>
//Temp port for now
#define USER_FILE_PATH "listOfUsers.txt"
#define FOLLOWERS_FILE_PATH "listOfFollowers.txt"
#define RECEIVES_TWEETS_FILE_PATH "receivedTweets.txt"
#define PENDING_TWEETS_FILE_PATH "pendingTweets.txt"

//bbeans (temp comment to open up pull req)

int PORT = 4001;

// void print_this(std::string s) {
//     std::cout << s << std::endl << std::flush;
// }

std::condition_variable listenerPitstopCV;
std::mutex listenerProceedMUT;

class userType {
    public:
    std::string userName;
    uint32_t userID;
    userType(std::string n, uint32_t id) {
        userName = n;
        userID = id;
    }
    userType() {}

    friend std::ostream& operator<<(std::ostream& os, const userType& user);
    friend std::istream& operator>>(std::istream& is, userType& user);
};

std::ostream& operator<<(std::ostream& os, const userType& user) {
    const size_t nameSize = user.userName.size();
    os << nameSize;
    os.write(user.userName.data(), nameSize);
    os << user.userID;
    os << '.';
    return os;
}
std::istream& operator>>(std::istream& is, userType& user){
    size_t nameSize = 0;
    is >> nameSize;
    user.userName.resize(nameSize);
    is.read(&user.userName[0], nameSize);
    is >> user.userID;
    char temp;
    is >> temp;
    return is;
}

class tweetData {
    public:
    uint32_t authorID;
    uint64_t tweetID;
    uint64_t timestamp;
    char _payload[256];
    uint32_t numRecipientsRemaining;

    friend std::ostream& operator<<(std::ostream& os, const tweetData& tweet);
    friend std::istream& operator>>(std::istream& is, tweetData& tweet);
};

std::ostream& operator<<(std::ostream& os, const tweetData& tweet) {
    os << tweet.authorID << '.'
    << tweet.tweetID << '.'
    << tweet.timestamp << '.';

    os.write(tweet._payload, 256);

    os << '.' << tweet.numRecipientsRemaining << '.';

    return os;
}
std::istream& operator>>(std::istream& is, tweetData& tweet) {
    char numBreaker;
    is >> tweet.authorID >> numBreaker
    >> tweet.tweetID >> numBreaker
    >> tweet.timestamp >> numBreaker;

    is.read(&tweet._payload[0], 256);

    is >> numBreaker >> tweet.numRecipientsRemaining >> numBreaker;

    return is;
}

class customBinarySemaphore {
    std::mutex m;
    int n = 1;
    bool canProceed = false;
    std::condition_variable cv;

    uint64_t curHolder = 0;
    uint64_t maxWaiter = 0;

    public:
    customBinarySemaphore() {;}
    customBinarySemaphore(customBinarySemaphore&&) {;}

    void P() {
        std::unique_lock<std::mutex> lk(m);
        uint64_t curAttempt = maxWaiter;
        maxWaiter++;

        if (n > 0 && curHolder == curAttempt) {
            n--;
        }
        else{
            cv.wait(lk, [this, curAttempt]{return canProceed && this->curHolder == curAttempt;});
            n--;
        }
        canProceed = (n == 0) ? false : true;
        lk.unlock();
    }
    void V() {
        std::unique_lock<std::mutex> lk(m);
        curHolder++;
        n++;
        canProceed = (n == 0) ? false : true;
        lk.unlock();
        cv.notify_all();
    }
};

class pendingTweet {
    public:
    uint32_t userAuthor;
    uint64_t tweetID; // Timestamp do dado
    pendingTweet(uint32_t author, uint64_t id) {
        userAuthor = author;
        tweetID = id;
    }
    pendingTweet() {}

    friend std::ostream& operator<<(std::ostream& os, const pendingTweet& pTweet);
    friend std::istream& operator>>(std::istream& is, pendingTweet& pTweet);
};

std::ostream& operator<<(std::ostream& os, const pendingTweet& pTweet){
    os << pTweet.tweetID << '.' << pTweet.userAuthor << '.';

    return os;
}
std::istream& operator>>(std::istream& is, pendingTweet& pTweet) {



    char numBreaker;
    is >> pTweet.tweetID >> numBreaker >> pTweet.userAuthor >> numBreaker;

    return is;
}



class transactionType {
    private:
    int transactionType;
    int SID;
    int curMaxSID;
    uint32_t curUserId;
    char targetUserName[256];
    tweetData tweet;
    uint64_t timestamp;
    uint32_t tweet_timestamp;

    bool duplicateTweet = false;
    bool tearDownDuplication = false;
    int duplicateSID;

    public:

    void setTimestamp(uint64_t ts) {this->timestamp = ts;}
    uint64_t getTimestamp() {return this->timestamp;}
    int getTransactionType() {return this->transactionType;}
    uint32_t getCurUserId() {return this->curUserId;}
    char* getTargetUsername() {return this->targetUserName;}
    tweetData getTweetData() {return this->tweet;}
    int getCurSessionSID() {return this->SID;}
    void setCurSessionSID(int _SID) {this->SID = _SID;}
    int getCurMaxSID() {return this->curMaxSID;}
    void setCurMaxSID(int _SID) {this->curMaxSID = _SID;}

    void registerDuplication(int _duplicateSID) {this->duplicateSID = _duplicateSID; this->duplicateTweet = true;}
    void registerDuplicationTeardown() {this->tearDownDuplication = true;}
    bool shouldTweetBeDuplicated() {return duplicateTweet;}
    bool shouldDuplicationTeardown() {return tearDownDuplication;}
    int getDuplicateSessionSID() {return duplicateSID;}

    void configurePostFollow(std::string _targetUserName, int _curUserId, uint32_t _tweet_timestamp) {
        this->transactionType = 0;
        strcpy(targetUserName, _targetUserName.c_str());
        this->curUserId = _curUserId;
        this->tweet_timestamp = _tweet_timestamp;
    }

    void configurePostUpdate(int _curUserId, tweetData _tweet) {
        this->transactionType = 1;
        this->curUserId = _curUserId;
        this->tweet = _tweet;
    }

    void configureRetrieveTweets(int _userID, int _SID) {
        this->transactionType = 2;
        this->curUserId = _userID;
        this->SID = _SID;
    }

    void configureRetrieveTweets(int _userID, int _SID, int _duplicateSID) {
        this->transactionType = 2;
        this->curUserId = _userID;
        this->SID = _SID;
        this->duplicateSID = _duplicateSID;
        this->duplicateTweet = true;
    }

    void configureDropConnection(int _userID, int _SID) {
        this->transactionType = 3;
        this->curUserId = _userID;
        this->SID = _SID;
    }

    void configureConstructConnection(int _userID, int _SID) {
        this->transactionType = 4;
        this->curUserId = _userID;
        this->SID = _SID;
    }
};

class ackType {
    public:
    int timestamp = 0;
    int cnt = 0;
};


struct connectionCounterType {
    uint32_t userID;
    uint8_t numConnections;
    connectionCounterType(uint32_t id, uint8_t nc) {
        this->userID = id;
        this->numConnections = nc;
    }
};

struct connectionTrackerType {
    uint32_t userID;
    int SID;    //Stands for SessionID
    std::vector<tweetData> userDuplicatedTweets;
    connectionTrackerType(uint32_t id, int _SID) {
        this->userID = id;
        this->SID = _SID;
    }
};

class RM_location_info{
    public:
    std::string hostname;
    int port;
    RM_location_info(std::string _h, int _p) {this->hostname = _h; this->port = _p;}
};


class connectionManager {
    private:
    customBinarySemaphore accessDB;
    customBinarySemaphore accessSID;
    std::vector<connectionCounterType> listOfConnectedUsers; //Uses the userID to identify connected users.
    std::vector<connectionTrackerType> listOfConnectedSessions;
    int curMaxSID = 0;


    public:
    bool registerConnection(int userID, int *SID);
    bool registerConnection(uint32_t userID, int SID);
    bool registerReconnection(int *userID, int SID);
    bool closeConnection(uint32_t userID, int SID);

    int getConnectionIndex(uint32_t userID);
    int getSessionIndex(uint32_t userID, int SID);
    int getDuplicateSessionIndex(uint32_t userID, int SID);
    bool doesClientHaveTwoConnections(uint32_t userID);

    bool registerDuplicateTweet(uint32_t userID, int SID, tweetData tweet);
    std::vector<tweetData> retrieveDuplicateTweet(uint32_t userID, int SID);
    bool doesClientHaveDuplicateTweets(uint32_t userID, int SID);

    void setLOCU(std::vector<connectionCounterType> _LOCU) {this->listOfConnectedUsers = _LOCU;}
    void setLOCS(std::vector<connectionTrackerType> _LOCS) {this->listOfConnectedSessions = _LOCS;}

    void setCurMaxSID(int _SID) {this->curMaxSID = _SID;}
    int getCurMaxSID() {return this->curMaxSID;}

    void copyManager(connectionManager *cm);


};

bool connectionManager::registerConnection(int userID, int *SID) {
    accessSID.P();
    uint64_t curSID = curMaxSID;
    curMaxSID++;
    accessSID.V();

    *SID = curSID;

    return this->registerConnection(userID, curSID);
}

bool connectionManager::registerConnection(uint32_t userID, int SID) {
    

    bool loopCond = true;

    accessSID.P();
    if(SID > curMaxSID) curMaxSID = SID + 1;
    accessSID.V();


    this->accessDB.P();
    std::vector<connectionCounterType>::iterator itr = this->listOfConnectedUsers.begin();


    while (loopCond) {
        if(itr == this->listOfConnectedUsers.end()) {
                loopCond = false;
        }
        else if((*itr).userID == userID) {
            if((*itr).numConnections < 2) {(*itr).numConnections++; this->listOfConnectedSessions.push_back(connectionTrackerType(userID, SID));
                                            this->accessDB.V(); return true;}
            else {this->accessDB.V(); return false;}
        }

        else {
            itr++;
        }
    }

    this->listOfConnectedUsers.push_back(connectionCounterType(userID, 1));
    this->listOfConnectedSessions.push_back(connectionTrackerType(userID, SID));
    print_this("This is a test: " + std::to_string(listOfConnectedSessions[0].userDuplicatedTweets.size()));
    this->accessDB.V();
    return true;
}

bool connectionManager::registerReconnection(int *userID, int _SID) {
    bool loopCond = true;

    this->accessDB.P();
    std::vector<connectionTrackerType>::iterator itr = this->listOfConnectedSessions.begin();

    while (loopCond) {


        if(itr == this->listOfConnectedSessions.end()) {
                loopCond = false;
        }
        else if((*itr).SID == _SID) {
            *userID = (*itr).userID;
            return true;
        }

        else {
            itr++;
        }
    }   

    this->accessDB.V();
    return false;
}

bool connectionManager::closeConnection(uint32_t userID, int SID) {
    bool loopCond = true;
    int sessionIndex = this->getSessionIndex(userID, SID);
    int connectionIndex = this->getConnectionIndex(userID);

    this->accessDB.P();

    std::vector<connectionCounterType>::iterator itr = this->listOfConnectedUsers.begin();

    while (loopCond) {

        if((*itr).userID == userID) {
            if((*itr).numConnections == 2) {
                (*itr).numConnections--;
                print_this("Closed on two");

                this->listOfConnectedSessions.erase(this->listOfConnectedSessions.begin() + sessionIndex);

                this->accessDB.V();
                return true;
            }
            else {
                print_this("Closed on one");
                int connectionIndex = itr - this->listOfConnectedUsers.begin();

                this->listOfConnectedUsers.erase(this->listOfConnectedUsers.begin() + connectionIndex);
                this->listOfConnectedSessions.erase(this->listOfConnectedSessions.begin() + sessionIndex);
                this->accessDB.V();
                return true;
            }
        }

        else {
            itr++;
            if(itr == this->listOfConnectedUsers.end()) {
                print_this("Closed on not found");
                loopCond = false;
            }
        }
    }

    this->accessDB.V();
    return false;
}

int connectionManager::getSessionIndex(uint32_t userID, int SID) {
    int i = 0;
    this->accessDB.P();
    while (i < this->listOfConnectedSessions.size()) {
        if (this->listOfConnectedSessions[i].userID == userID &&
            this->listOfConnectedSessions[i].SID == SID)  { this->accessDB.V(); return i;}

        i++;
    }
    this->accessDB.V();
    return -1;
}

int connectionManager::getDuplicateSessionIndex(uint32_t userID, int SID) {
    int i = 0;
    this->accessDB.P();
    while (i < this->listOfConnectedSessions.size()) {
        if (this->listOfConnectedSessions[i].userID == userID &&
            this->listOfConnectedSessions[i].SID != SID) { this->accessDB.V(); return i;}

        i++;
    }
    this->accessDB.V();
    return -1;
}


int connectionManager::getConnectionIndex(uint32_t userID) {
    int i = 0;
    this->accessDB.P();
    while (i < this->listOfConnectedUsers.size()) {
        if (this->listOfConnectedUsers[i].userID == userID) { this->accessDB.V(); return i;}

        i++;
    }
    this->accessDB.V();
    return -1;
}

bool connectionManager::doesClientHaveTwoConnections(uint32_t userID) {
    int connectionIndex = this->getConnectionIndex(userID);
    bool answer;

    this->accessDB.P();
    if(connectionIndex == -1) answer = false;
    else answer = this->listOfConnectedUsers[connectionIndex].numConnections == 2;
    print_this("This is hell dude. " + std::to_string(this->listOfConnectedUsers[connectionIndex].numConnections));
    this->accessDB.V();

    return answer;
}

bool connectionManager::registerDuplicateTweet(uint32_t userID, int SID, tweetData tweet) {
    int sessionIndex = this->getDuplicateSessionIndex(userID, SID);

    this->accessDB.P();
    this->listOfConnectedSessions[sessionIndex].userDuplicatedTweets.push_back(tweet);
    this->accessDB.V();

    return true;
}

std::vector<tweetData> connectionManager::retrieveDuplicateTweet(uint32_t userID, int SID) {

    int sessionIndex = this->getSessionIndex(userID, SID);
    std::vector<tweetData> returnTweet;

    if (sessionIndex == -1) return returnTweet;

    this->accessDB.P();
    returnTweet = this->listOfConnectedSessions[sessionIndex].userDuplicatedTweets;
    this->listOfConnectedSessions[sessionIndex].userDuplicatedTweets.clear();
    this->accessDB.V();

    return returnTweet;
}

bool connectionManager::doesClientHaveDuplicateTweets(uint32_t userID, int SID) {
    if (userID == -1) return false;

    int sessionIndex = this->getSessionIndex(userID, SID);
    if (sessionIndex == -1) return true;

    bool answer;

    this->accessDB.P();
    answer = this->listOfConnectedSessions[sessionIndex].userDuplicatedTweets.size() != 0;
    print_this(std::to_string(this->listOfConnectedSessions[sessionIndex].userDuplicatedTweets.size()));
    std::string s(this->listOfConnectedSessions[sessionIndex].userDuplicatedTweets[0]._payload);
    print_this(s);
    this->accessDB.V();

    return answer;
}

void connectionManager::copyManager(connectionManager *cm) {
    this->accessDB.P();

    (*cm).setLOCU(this->listOfConnectedUsers);
    (*cm).setLOCS(this->listOfConnectedSessions);

    this->accessDB.V();
}


class databaseManager {
    std::vector<userType> listOfUsers;;
    std::vector<std::vector<int>> listOfFollowers;
    std::vector<std::vector<tweetData>> listOfReceivedTweets;
    std::vector<std::vector<pendingTweet>> listOfPendingTweets;
    int numTweets = 0;
    //All the above vectors are accessed using semaphores and reader-writer logic.
    //LOU is reader-preferred, LOF is reader preferred, LORT is reader preferred, LOPT is writer-preferred.

    customBinarySemaphore LOU_rw_sem;
    customBinarySemaphore LOF_rw_sem;
    customBinarySemaphore LORT_rw_sem;
    customBinarySemaphore LOPT_rw_sem;
    customBinarySemaphore LOPT_readTry;
    customBinarySemaphore LOPT_eraseTry;
    customBinarySemaphore query_try;
    customBinarySemaphore DB_access_sem;

    customBinarySemaphore tweetnum_sem;
    customBinarySemaphore query_cnt_sem;

    std::mutex LOU_cnt_mutex;
    std::mutex LOF_cnt_mutex;
    std::mutex LORT_cnt_mutex;
    std::mutex LOPT_w_cnt_mut;
    std::mutex LOPT_r_cnt_mut;
    std::mutex LOPT_duplicate_mut;

    std::condition_variable LOPT_duplicate_cv;

    connectionManager *cm_temp;

    public:
    databaseManager(connectionManager *_cm) {this->cm_temp = _cm;}

    bool saveDatabase();
    bool loadDatabase();

    bool addUser(std::string name);
    int getUserIndex(std::string name);
    std::string getUserName(int userID);
    std::vector<int> getUserFollowers(int userID);
    std::vector<int> getPendingTweetAuthors(int userID);
    int getNumUsers() {return this->listOfUsers.size();}

    bool doesClientHavePendingTweets(int userID, int SID);
    bool postFollow(std::string targetUserName, int curUserID, time_t timestamp);
    bool postUpdate(int userID, tweetData tweet);
    std::vector<tweetData> retrieveTweetsFromFollowed(int userID, int SID, bool *tweetsDuplicated);
    std::vector<tweetData> retrieveTweetsFromFollowed(int userID, int SID);

    void setLOU(std::vector<userType> _lou) {this->listOfUsers = _lou;}
    void setLOF(std::vector<std::vector<int>> _lof) {this->listOfFollowers = _lof;}
    void setLORT(std::vector<std::vector<tweetData>> _lort) {this->listOfReceivedTweets = _lort;}
    void setLOPT(std::vector<std::vector<pendingTweet>> _lopt) {this->listOfPendingTweets = _lopt;}

    void copyDatabase(databaseManager *db);

    private:

    bool _alreadyFollowed(int targetUserID, int curUserID);
    bool _registerUpdate(int curUserID, tweetData tweet);
    bool _forwardUpdateToFollowers(int curUserID, uint64_t tweetID);
    std::vector<pendingTweet> _retrievePendingTweets(int userID);
    int _getNumFollowers(int curUserID);
    bool _clearUsersPendingTweets(int curUserID);
    bool _updateReceivedTweet(int targetUserID, uint64_t tweetID);
    bool _handleLateFollow(int targetUserID, int curUserID, time_t timestamp);

    int saveListOfUsers();
    int loadListOfUsers();
    int loadListOfFollowers();
    int saveListOfFollowers();
    int saveListOfReceivedTweets();
    int loadListOfReceivedTweets();
    int saveListOfPendingTweets();
    int loadListOfPendingTweets();

     int LOU_cnt = 0;
     int LOF_cnt = 0;
     int LORT_cnt = 0;
     int LOPT_r_cnt = 0;
     int LOPT_w_cnt = 0;

     int query_cnt = 0;
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
    return true; // to avoid warning message
}

int databaseManager::saveListOfPendingTweets(){

    std::ofstream file_obj;
    file_obj.open(PENDING_TWEETS_FILE_PATH);

    int numUsers = this->listOfPendingTweets.size();

    file_obj << numUsers << '.';

    for(int i = 0; i < numUsers; i++) {

        int numTweets = this->listOfPendingTweets[i].size();
        file_obj << numTweets << '/';

        for(const auto &tweetIterator : this->listOfPendingTweets[i]) file_obj << tweetIterator << '\n';
    }

    file_obj.close();

    return 0;
}

int databaseManager::loadListOfPendingTweets(){

    std::ifstream inFile;
    inFile.open(PENDING_TWEETS_FILE_PATH);

    int numUsers;
    char numBreaker;

    inFile >> numUsers >> numBreaker;

    for(int i =0; i < numUsers; i++) {

        this->listOfPendingTweets.push_back(std::vector<pendingTweet>());
        int numTweets;
        inFile >> numTweets >> numBreaker;

        for(int j = 0; j < numTweets; j++) {
            pendingTweet tweet;
            inFile >> tweet >> numBreaker;
            this->listOfPendingTweets[i].push_back(tweet);
        }

    }

    inFile.close();
    return 0; //avoid warning message
}

int databaseManager::saveListOfReceivedTweets(){

    std::ofstream file_obj;
    file_obj.open(RECEIVES_TWEETS_FILE_PATH);

    int numUsers = this->listOfReceivedTweets.size();

    file_obj << numUsers << '.';
    file_obj << this->numTweets << '.';

    for(int i = 0; i < numUsers; i++) {

        int numTweets = this->listOfReceivedTweets[i].size();
        file_obj << numTweets << '/';

        for(const auto &tweetIterator : this->listOfReceivedTweets[i]) file_obj << tweetIterator << '\n';
    }

    file_obj.close();

    return 0;
}

int databaseManager::loadListOfReceivedTweets(){

    std::ifstream inFile;
    inFile.open(RECEIVES_TWEETS_FILE_PATH);

    int numUsers;
    char numBreaker;

    inFile >> numUsers >> numBreaker;
    inFile >> this->numTweets >> numBreaker;

    for(int i =0; i < numUsers; i++) {

        this->listOfReceivedTweets.push_back(std::vector<tweetData>());
        int numTweets;
        inFile >> numTweets >> numBreaker;

        for(int j = 0; j < numTweets; j++) {
            tweetData tweet;
            inFile >> tweet >> numBreaker;
            this->listOfReceivedTweets[i].push_back(tweet);
        }

    }

    inFile.close();
    return 0; // avoid warning message
}

int databaseManager::loadListOfFollowers(){
    std::ifstream infile(FOLLOWERS_FILE_PATH);

    int numUsers;
    char numBreaker;

    infile >> numUsers >> numBreaker;

    for (int i = 0; i < numUsers; i++) {

        this->listOfFollowers.push_back(std::vector<int>());
        int numFollowers;
        infile >> numFollowers >> numBreaker;

        for(int j = 0; j < numFollowers; j++) {
            int followerID;
            infile >> followerID >> numBreaker;
            this->listOfFollowers[i].push_back(followerID);
        }
    }

   infile.close();


    return 0;

}

int databaseManager::saveListOfFollowers(){
    std::ofstream file_obj;
    file_obj.open(FOLLOWERS_FILE_PATH);

    int numUsers = this->listOfFollowers.size();

    file_obj << numUsers << '.';

    for (int i = 0; i < numUsers; i++) {

        int numFollowers = this->listOfFollowers[i].size();
        file_obj << numFollowers << '.';

        for(const auto &innerIterator : this->listOfFollowers[i]) file_obj << innerIterator << '/';
    }

//    for(std::vector<int> lineOfFollowers : this->listOfFollowers){
//        file_obj.write((char*) &lineOfFollowers, sizeof(lineOfFollowers));
//    }

    file_obj.close();

    return 0;
}

int databaseManager::saveListOfUsers(){

    std::ofstream file_obj;
    file_obj.open(USER_FILE_PATH, std::ios::trunc);

    int numUsers = this->listOfFollowers.size();

    file_obj << numUsers << '.';

    for(auto user : this->listOfUsers){
        file_obj << user;
    }
    file_obj.close();
    return 0;
}

int databaseManager::loadListOfUsers(){
    std::ifstream file_obj;
    file_obj.open(USER_FILE_PATH,std::ios::in);

    int numUsers;
    char numBreaker;

    file_obj >> numUsers >> numBreaker;

    for(int i = 0; i < numUsers; i++){
        userType user;
        file_obj >> user;
        this->listOfUsers.push_back(user);
    }
    file_obj.close();
    return 0;
}

bool databaseManager::saveDatabase(){

    this->query_try.P();
    this->DB_access_sem.P();

    this->saveListOfUsers();
    this->saveListOfFollowers();
    this->saveListOfPendingTweets();
    this->saveListOfReceivedTweets();

    this->DB_access_sem.V();
    this->query_try.V();

    return true;
}

bool databaseManager::loadDatabase(){

    this->query_try.P();
    this->DB_access_sem.P();

    this->listOfFollowers.clear();
    this->listOfPendingTweets.clear();
    this->listOfReceivedTweets.clear();
    this->listOfUsers.clear();

    this->loadListOfFollowers();
    this->loadListOfPendingTweets();
    this->loadListOfReceivedTweets();
    this->loadListOfUsers();

    this->DB_access_sem.V();
    this->query_try.V();

    return true;
}

void databaseManager::copyDatabase(databaseManager *db){
    this->query_try.P();
    this->DB_access_sem.P();

    (*db).setLOU(this->listOfUsers);
    (*db).setLOF(this->listOfFollowers);
    (*db).setLORT(this->listOfReceivedTweets);
    (*db).setLOPT(this->listOfPendingTweets);

    this->DB_access_sem.V();
    this->query_try.V();
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

std::string databaseManager::getUserName(int userID) {

    std::string userName;

    std::unique_lock<std::mutex> lk(this->LOU_cnt_mutex);
    this->LOU_cnt++;
    if(this->LOU_cnt == 1) this->LOU_rw_sem.P();
    lk.unlock();

    userName = listOfUsers[userID].userName;

    lk.lock();
    this->LOU_cnt--;
    if(this->LOU_cnt == 0) this->LOU_rw_sem.V();
    lk.unlock();

    return userName;
}

 std::vector<int> databaseManager::getUserFollowers(int userID) {
    std::unique_lock<std::mutex> lk(this->LOF_cnt_mutex);
    this->LOF_cnt++;
    if(this->LOF_cnt == 1) this->LOF_rw_sem.P();
    lk.unlock();

    std::vector<int> returnList = this->listOfFollowers[userID];

    lk.lock();
    this->LOF_cnt--;
    if(this->LOF_cnt == 0) this->LOF_rw_sem.V();
    lk.unlock();

    return returnList;
 }

bool databaseManager::doesClientHavePendingTweets(int userID, int SID) {

    //Timestamp transaction request time
    //Check whether userID's pending tweets can be read
    //If yes, lock the drawer for reading
    //Timestamp transaction start time //// Alternatively, timestamp finish.

    if(userID == -1) {
        return false;
    }

    //WRiters-preferred read operation
    this->LOPT_readTry.P();
    std::unique_lock<std::mutex> lk_r_m(this->LOPT_r_cnt_mut);
    this->LOPT_r_cnt++;
    if(this->LOPT_r_cnt == 1) this->LOPT_rw_sem.P();
    lk_r_m.unlock();
    this->LOPT_readTry.V();

    bool answer = this->listOfPendingTweets[userID].size() != 0 || (*cm_temp).doesClientHaveDuplicateTweets(userID, SID);
//    std::cout << "Checked whether client has pending tweets " << this->listOfPendingTweets[userID].size()  << std::endl << std::flush;

    lk_r_m.lock();
    this->LOPT_r_cnt--;
    if(this->LOPT_r_cnt == 0) this->LOPT_rw_sem.V();
    lk_r_m.unlock();

    //Commit change to RMs
    //If all RMs accept, commit to permanent storage.
    //Otherwise, error.
    //Release lock if appropriate

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
//    std::cout<< "Followers: " << this->listOfFollowers[curUserID].size() << std::endl << std::flush;

    lk.lock();
    this->LOF_cnt--;
    if(this->LOF_cnt == 0) this->LOF_rw_sem.V();
    lk.unlock();

    return numberOfFollowers;
}

std::vector<int> databaseManager::getPendingTweetAuthors(int userID) {

    this->LOPT_readTry.P();
    std::unique_lock<std::mutex> lk_r_m(this->LOPT_r_cnt_mut);
    this->LOPT_r_cnt++;
    if(this->LOPT_r_cnt == 1) this->LOPT_rw_sem.P();
    lk_r_m.unlock();
    this->LOPT_readTry.V();

    std::vector<pendingTweet> listOfPTweets = this->listOfPendingTweets[userID];

    lk_r_m.lock();
    this->LOPT_r_cnt--;
    if(this->LOPT_r_cnt == 0) this->LOPT_rw_sem.V();
    lk_r_m.unlock();

    std::vector<int> returnList;
    for (int i = 0; i < listOfPTweets.size(); i++) {
        if (std::find(returnList.begin(), returnList.end(), listOfPTweets[i].userAuthor)==returnList.end())
            returnList.push_back(listOfPTweets[i].userAuthor);
    }

    return returnList;

}

bool databaseManager::postFollow(std::string targetUserName, int curUserID, time_t timestamp) {

    //Timestamp transaction request time
    //Check if list of followers can be written into
    //If yes, lock drawer for writing. (WHAT ABT TIMESTAMP MY DUDE??? in what order will they be unlocked from waiting?)
    //Else, wait for your turn.
    //Timestamp transaction begin time


    this->query_try.P();
    this->query_cnt_sem.P();
    this->query_cnt++;
    if(this->query_cnt == 1) this->DB_access_sem.P();
    this->query_cnt_sem.V();
    this->query_try.V();

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

    this->query_cnt_sem.P();
    this->query_cnt--;
    if(this->query_cnt == 0) this->DB_access_sem.V();
    this->query_cnt_sem.V();

    //Commit changes to RMs
    //If they all ack, commit to database
    //Release locks if appropriate

    //Check if there are any tweets that have been posted after follow timestamp but before it's finished registering the follow.

    return true;
}

bool databaseManager::_registerUpdate(int curUserID, tweetData tweet) {

    tweet.numRecipientsRemaining = this->_getNumFollowers(curUserID);

    this->LORT_rw_sem.P();
    this->listOfReceivedTweets[curUserID].push_back(tweet);
    std::cout<< "Registered tweet: " << tweet._payload << std::endl << std::flush;
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
//        std::cout << targetUserID << std::endl << std::flush;
    }
    this->LOPT_rw_sem.V();

    lk_w_cnt.lock();
    this->LOPT_w_cnt--;
    if(this->LOPT_w_cnt == 0) this->LOPT_readTry.V();
    lk_w_cnt.unlock();

    return true;
}

bool databaseManager::_clearUsersPendingTweets(int curUserID) {

    std::unique_lock<std::mutex> lk_w_cnt(this->LOPT_w_cnt_mut);
    this->LOPT_w_cnt++;
    if(this->LOPT_w_cnt == 1) this->LOPT_readTry.P();
    lk_w_cnt.unlock();

    this->LOPT_rw_sem.P();

    this->listOfPendingTweets[curUserID].clear();

    this->LOPT_rw_sem.V();

    lk_w_cnt.lock();
    this->LOPT_w_cnt--;
    if(this->LOPT_w_cnt == 0) this->LOPT_readTry.V();
    lk_w_cnt.unlock();

    return true;
}

bool databaseManager::postUpdate(int userID, tweetData tweet){

    //Timestamp transaction request time
    //Check if list of followers can be read.
    //Check if list of received tweets can be written into.
    //Check if list of pending tweets can be written into.
    //If yes, lock drawer for writing. (WHAT ABT TIMESTAMP MY DUDE??? in what order will they be unlocked from waiting?)
    //Else, wait for your turn.
    //Timestamp transaction begin time.


    this->query_try.P();
    this->query_cnt_sem.P();
    this->query_cnt++;
    if(this->query_cnt == 1) this->DB_access_sem.P();
    this->query_cnt_sem.V();
    this->query_try.V();

    //Complete the tweet metadata with the number of followers this user has.
    this->tweetnum_sem.P();
    tweet.tweetID = this->numTweets;
    numTweets++;
    this->tweetnum_sem.V();

    if(this->_getNumFollowers(userID) != 0) {

        std::unique_lock<std::mutex> lk_u(listenerProceedMUT);
        //Put the tweet in the list of received tweets
        if(!this->_registerUpdate(userID, tweet)) return false;

        //For each user in said list, add pendingTweet regarding current tweet
        if(!this->_forwardUpdateToFollowers(userID, tweet.tweetID)) return false;
            //! weh. Make a function to remove the update if it fails to add it.

        lk_u.unlock();
        listenerPitstopCV.notify_all();
    }

    this->query_cnt_sem.P();
    this->query_cnt--;
    if(this->query_cnt == 0) this->DB_access_sem.V();
    this->query_cnt_sem.V();

    //Commit changes to RMs
    //If they all ack, commit to database
    //Release locks if appropriate

    return true;
}

std::vector<pendingTweet> databaseManager::_retrievePendingTweets(int userID){

    std::vector<pendingTweet> returnList;

    //Writers-preferred atomic and exclusive read+modify operation.
    //Have lower priority than normal write operation, but if allowed to execute, prevent any other reader from accessing
    //before it is finished modifying.
    this->LOPT_readTry.P();
    std::unique_lock<std::mutex> lk_r_m(this->LOPT_r_cnt_mut);
    this->LOPT_r_cnt++;
    if(this->LOPT_r_cnt == 1) this->LOPT_rw_sem.P();
    lk_r_m.unlock();

    returnList = this->listOfPendingTweets[userID];
    this->listOfPendingTweets[userID].clear();

    this->LOPT_readTry.V();

    lk_r_m.lock();
    this->LOPT_r_cnt--;
    if(this->LOPT_r_cnt == 0) this->LOPT_rw_sem.V();
    lk_r_m.unlock();

    return returnList;
}

bool databaseManager::_updateReceivedTweet(int targetUserID, uint64_t tweetID) {

    this->LORT_rw_sem.P();

    int i = 0;
    bool loopCond = true;

    while(loopCond == true) {
        if(i >=this->listOfReceivedTweets[targetUserID].size()) loopCond = false;

        else if(this->listOfReceivedTweets[targetUserID][i].tweetID == tweetID) {
            this->listOfReceivedTweets[targetUserID][i].numRecipientsRemaining--;
            if(this->listOfReceivedTweets[targetUserID][i].numRecipientsRemaining == 0) {
                this->listOfReceivedTweets[targetUserID].erase(this->listOfReceivedTweets[targetUserID].begin() + i);
            }
            loopCond = false;
        }
        else {
            i++;
        }
    }

    this->LORT_rw_sem.V();

    return true;

}


std::vector<tweetData> databaseManager::retrieveTweetsFromFollowed(int userID, int SID){
    bool temp;
    return retrieveTweetsFromFollowed(userID, SID, &temp);
}

std::vector<tweetData> databaseManager::retrieveTweetsFromFollowed(int userID, int SID, bool *tweetsDuplicated){

    //Timestamp transaction request time
    //Check if list of received tweets can be written into.
    //Check if list of pending tweets can be written into.
    //If yes, lock drawer for writing. (WHAT ABT TIMESTAMP MY DUDE??? in what order will they be unlocked from waiting?)
    //Else, wait for your turn.
    //Timestamp transaction begin time.

    this->query_try.P();
    this->query_cnt_sem.P();
    this->query_cnt++;
    if(this->query_cnt == 1) this->DB_access_sem.P();
    this->query_cnt_sem.V();
    this->query_try.V();

    std::vector<tweetData> receivedTweets;
    std::vector<pendingTweet> pendingTweets = this->_retrievePendingTweets(userID);
//    this->_clearUsersPendingTweets(userID);

    //If this function was called, but before it could get the user's tweets there was another connection from same user who already
    //retrieved the pending tweets, look at the duplicate tweets vector inside the connectionManager

    print_this("Retrieved pending tweets inside function");

    std::unique_lock<std::mutex> lk(this->LORT_cnt_mutex);
    this->LORT_cnt++;
    if(this->LORT_cnt == 1) this->LORT_rw_sem.P();
    lk.unlock();

    if(pendingTweets.size() == 0) {
        print_this("ATTEMPTED TO BINGUS");
        std::unique_lock<std::mutex> lk(this->LOPT_duplicate_mut);
        this->LOPT_duplicate_cv.wait_for(lk, std::chrono::seconds(10),
                                            [this, userID, SID]{return (*cm_temp).doesClientHaveDuplicateTweets(userID, SID);});
        lk.unlock();
        receivedTweets = (*cm_temp).retrieveDuplicateTweet(userID, SID);
    }

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

    print_this("Retrieved received tweets inside function");
    if(pendingTweets.size() != 0 && (*cm_temp).doesClientHaveTwoConnections(userID)) {
        print_this("Got inside check");
        for (int i = 0; i < receivedTweets.size(); i++) {
            (*cm_temp).registerDuplicateTweet(userID, SID, receivedTweets[i]);
        }
        print_this("Finished duplicating tweets");
        *tweetsDuplicated = true;
        this->LOPT_duplicate_cv.notify_all();
        print_this("Finished notifying");
    }

    print_this("Got through check");

    for(int i = 0; i < pendingTweets.size(); i++) {
        this->_updateReceivedTweet(pendingTweets[i].userAuthor, pendingTweets[i].tweetID);
    }


    this->query_cnt_sem.P();
    this->query_cnt--;
    if(this->query_cnt == 0) this->DB_access_sem.V();
    this->query_cnt_sem.V();

    //Commit changes to RMs
    //If they all ack, commit to database
    //Release locks if appropriate

    return receivedTweets;

}



class transactionManager {
    private:
    std::vector<customBinarySemaphore> resource_listOfFollowers;
    std::vector<customBinarySemaphore> resource_listOfReceivedTweets;
    std::vector<customBinarySemaphore> resource_listOfPendingTweets;

    //0 - 15 / 1 - 20 / 2 - 25

    customBinarySemaphore resource_pendingTweetAccess;
    customBinarySemaphore resource_receivedTweetAccess;
    customBinarySemaphore timestamp_sempahore;
    customBinarySemaphore resource_curMaxSID;
    int timestampCounter = 0;

    RM_info *primary_RM_socket;
    RM_info selfSocket;
    std::vector<RM_info> *secondary_RM_sockets;
    std::vector<customBinarySemaphore> secondary_RM_access;

    std::vector<uint64_t> receivedAcks;
    customBinarySemaphore resource_receivedAcks;

    std::vector<ackType> unprocessedAcks;

    databaseManager *db_temp;
    connectionManager *cm_temp;

    public:
    transactionManager(databaseManager *_db, connectionManager *_cm) {this->db_temp = _db; this->cm_temp = _cm;}

    void applyTimestamp(transactionType* t);
    bool executeTransaction(std::string targetUser, int userID, time_t follow_timestamp);
    bool executeTransaction(int userID, tweetData tweet);
    bool executeTransaction(int userID, int SID, std::vector<tweetData> *resultVec);
    bool executeConnection(int userID, int *SID);
    bool executeDisconnection(int userID, int SID);
    bool propagateTransactionToBackups(transactionType transaction);
    void listenToSecondaryServers(bool* shutdownNotice, bool* RM_shutdownNotice);
    void configureServerInfo(std::vector<RM_info> *secondary_server_sockets, RM_info *primary_server_socket, RM_info _selfSocket);
    void configureResources();
    void listenInSecondaryMode(bool* shutdownNotice, bool*( RM_shutdownNotice));
};

void transactionManager::configureResources() {
    int nUsers = (*db_temp).getNumUsers();

    print_this("Number of users spotted: " + std::to_string(nUsers));

    for (int i = 0; i < nUsers; i++) {
        this->resource_listOfFollowers.push_back(customBinarySemaphore());
        this->resource_listOfReceivedTweets.push_back(customBinarySemaphore());
        this->resource_listOfPendingTweets.push_back(customBinarySemaphore());
    }
}

void transactionManager::applyTimestamp(transactionType* t) {
    timestamp_sempahore.P();
    t->setTimestamp(this->timestampCounter);
    timestampCounter++;
    timestamp_sempahore.V();
}

bool transactionManager::executeTransaction(std::string targetUser, int userID, time_t follow_timestamp) {

    print_this("Primary server has started reserving resources for postFollow routine.");

    transactionType transaction;
    int targetUserID = (*db_temp).getUserIndex(targetUser);

    resource_listOfFollowers[targetUserID].P();
    print_this("Primary server has reserved LOF.");

    applyTimestamp(&transaction); print_this("Primary server has applied timestamp.");;
    transaction.configurePostFollow(targetUser, userID, follow_timestamp); print_this("Primary server has configured transaction.");
    transaction.setCurMaxSID((*cm_temp).getCurMaxSID());

    connectionManager cm_privateCopy;
    databaseManager privateCopy(&cm_privateCopy);
    (*db_temp).copyDatabase(&privateCopy); print_this("Primary server has created its private copy.");
    //Make privateCopy make a copy of db_temp;

    print_this("Primary server thread has allocated its resources.");

    bool result = privateCopy.postFollow(targetUser, userID, follow_timestamp);

    print_this("Primary server thread has finished operating on private copy. Preparing to propagate.");
    if (result == false){
        print_this("Primary server, user " + std::to_string(userID) + " attempted to follow user they already follow.");
        print_this("Preemptively ending transaction as processed, no need to propagate effectless transaction.");
        return true; //Error during operation on private copy
    }

    result = this->propagateTransactionToBackups(transaction);

    print_this("Primary server thread has finished propagating the transaction.");
    if (result == false){
        return false;
    }

    (*db_temp).postFollow(targetUser, userID, follow_timestamp);

    print_this("Primary server thread has finished integrating the transaction.");
    resource_listOfFollowers[targetUserID].V();

    return true;
}

bool transactionManager::executeTransaction(int userID, tweetData tweet) {

    transactionType transaction;

    print_this("Primary server has started reserving resources for Update routine.");

    resource_listOfFollowers[userID].P();
    print_this("Primary server has reserved LOF.");
    std::vector<int> listOfFollowers = (*db_temp).getUserFollowers(userID);
    resource_pendingTweetAccess.P();
    for (int i = 0; i < listOfFollowers.size(); i++) this->resource_listOfPendingTweets[listOfFollowers[i]].P();
    resource_pendingTweetAccess.V();
    print_this("Primary server has reserved LOPT.");
    resource_receivedTweetAccess.P(); resource_listOfReceivedTweets[userID].P();  resource_receivedTweetAccess.V();
    print_this("Primary server has reserved LORT.");

    applyTimestamp(&transaction); print_this("Primary server has applied timestamp.");
    transaction.configurePostUpdate(userID, tweet); print_this("Primary server has configured transaction.");
    transaction.setCurMaxSID((*cm_temp).getCurMaxSID());

    connectionManager cm_privateCopy;
    databaseManager privateCopy(&cm_privateCopy);
    (*db_temp).copyDatabase(&privateCopy);  print_this("Primary server has created its private copy.");
    //Make privateCopy make a copy of db_temp;

    print_this("Primary server thread has allocated its resources.");

    bool result = privateCopy.postUpdate(userID, tweet);
    print_this("Primary server thread has finished operating on private copy. Preparing to propagate.");
    if (result == false){
        return false; //Error during operation on private copy
    }

    result = this->propagateTransactionToBackups(transaction);
    print_this("Primary server thread has finished propagating the transaction.");
    if (result == false){
        return false;
    }


    (*db_temp).postUpdate(userID, tweet);

    print_this("Primary server thread has finished integrating the transaction.");


    resource_listOfReceivedTweets[userID].V();
    for (int i = 0; i < listOfFollowers.size(); i++) this->resource_listOfPendingTweets[listOfFollowers[i]].V();
    resource_listOfFollowers[userID].V();


    return true;
}

bool transactionManager::executeTransaction(int userID, int SID, std::vector<tweetData> *resultVec) {

    transactionType transaction;

    print_this("Primary server has started reserving resources for a retrieveTweets routine.");
    print_this("User making request: " + std::to_string(userID));

    resource_pendingTweetAccess.P(); resource_listOfPendingTweets[userID].P(); resource_pendingTweetAccess.V();
    print_this("Primary server has reserved LOPT.");
    std::vector<int> listOfTweetAuthors = (*db_temp).getPendingTweetAuthors(userID);
    resource_receivedTweetAccess.P();
    for (int i = 0; i < listOfTweetAuthors.size(); i++) {this->resource_listOfReceivedTweets[listOfTweetAuthors[i]].P();}
    resource_receivedTweetAccess.V();
    print_this("Primary server has reserved LORT.");

    applyTimestamp(&transaction); print_this("Primary server has applied timestamp.");
    transaction.configureRetrieveTweets(userID, SID); print_this("Primary server has configured transaction.");
    transaction.setCurMaxSID((*cm_temp).getCurMaxSID());

    connectionManager cm_privateCopy;
    (*cm_temp).copyManager(&cm_privateCopy);
    databaseManager db_privateCopy(&cm_privateCopy);
    (*db_temp).copyDatabase(&db_privateCopy); print_this("Primary server has created its private copy.");
    //Make privateCopy make a copy of db_temp;

    print_this("Primary server thread has allocated its resources.");


    bool tweetsDuplicated = false;
    db_privateCopy.retrieveTweetsFromFollowed(userID, SID, &tweetsDuplicated);
    print_this("Primary server thread has finished operating on private copy. Preparing to propagate.");

    if(tweetsDuplicated) {
        int duplicateSID = (*cm_temp).getDuplicateSessionIndex(userID, SID);
        transaction.registerDuplication(duplicateSID);
    }
    else if((*cm_temp).doesClientHaveTwoConnections(userID) == false){
        transaction.registerDuplicationTeardown();
    }

    bool result = this->propagateTransactionToBackups(transaction);
    if (result == false){
        return false;
    }
    print_this("Primary server thread has finished propagating the transaction.");

    *resultVec = (*db_temp).retrieveTweetsFromFollowed(userID, SID);

    print_this("Primary server thread has finished integrating the transaction.");

    for (int i = 0; i < listOfTweetAuthors.size(); i++) this->resource_listOfReceivedTweets[listOfTweetAuthors[i]].V();
    resource_listOfPendingTweets[userID].V();


    return true;
}

bool transactionManager::executeConnection(int userID, int *SID) {
    transactionType transaction;

    print_this("Primary server has started login propagation.");

    resource_curMaxSID.P();

    applyTimestamp(&transaction); print_this("Primary server has applied timestamp.");
    transaction.setCurMaxSID((*cm_temp).getCurMaxSID());

    connectionManager cm_privateCopy;
    (*cm_temp).copyManager(&cm_privateCopy);
    databaseManager db_privateCopy(&cm_privateCopy);
    (*db_temp).copyDatabase(&db_privateCopy); print_this("Primary server has created its private copy.");
    //Make privateCopy make a copy of db_temp;

    print_this("Primary server thread has allocated its resources.");

    int localSID = 0;
    bool success = cm_privateCopy.registerConnection(userID, &localSID);
    transaction.configureConstructConnection(userID, localSID); print_this("Primary server has configured transaction.");

    if (success = false) {
        return false;
    }
    print_this("Primary server thread has finished operating on private copy. Preparing to propagate.");

    bool result = this->propagateTransactionToBackups(transaction);
    if (result == false){
        return false;
    }
    print_this("Primary server thread has finished propagating the transaction.");

    (*cm_temp).registerConnection(userID, localSID);
    *SID = localSID;

    print_this("Primary server thread has finished integrating the transaction.");

    resource_curMaxSID.V();


    return true;
}

bool transactionManager::executeDisconnection(int userID, int SID) {
    transactionType transaction;

    print_this("Primary server has started disconnect propagation.");

    applyTimestamp(&transaction); print_this("Primary server has applied timestamp.");
    transaction.configureDropConnection(userID, SID); print_this("Primary server has configured transaction.");
    transaction.setCurMaxSID((*cm_temp).getCurMaxSID());

    connectionManager cm_privateCopy;
    (*cm_temp).copyManager(&cm_privateCopy);
    databaseManager db_privateCopy(&cm_privateCopy);
    (*db_temp).copyDatabase(&db_privateCopy); print_this("Primary server has created its private copy.");
    //Make privateCopy make a copy of db_temp;

    print_this("Primary server thread has allocated its resources.");

    bool success = cm_privateCopy.closeConnection(userID, SID);
    if (success = false) {
        return false;
    }
    print_this("Primary server thread has finished operating on private copy. Preparing to propagate.");

    bool result = this->propagateTransactionToBackups(transaction);
    if (result == false){
        return false;
    }
    print_this("Primary server thread has finished propagating the transaction.");

    (*cm_temp).closeConnection(userID, SID);

    print_this("Primary server thread has finished integrating the transaction.");

    return true;
}

bool transactionManager::propagateTransactionToBackups(transactionType transaction) {

    print_this("Testing: " + std::to_string((*secondary_RM_sockets).size()) + " " + 
                std::to_string(transaction.getTimestamp()));
    for(int i = 0; i < (*secondary_RM_sockets).size(); i++) {
        print_this("Primary server attempting to propagate to server " + std::to_string((*secondary_RM_sockets)[i].RM_id));
        secondary_RM_access[i].P();
        print_this("Primary server reserved channel to " + std::to_string((*secondary_RM_sockets)[i].RM_id));
        int bytes = write((*secondary_RM_sockets)[i].socketfd, &transaction, sizeof(transaction));
        print_this("Primary server attempted to write to " + std::to_string((*secondary_RM_sockets)[i].RM_id));
         if (bytes == -1) {
            print_this("SECONDARY SERVER UNAVAILABLE: " + i);
         }
         else if (bytes < sizeof(transaction)) {
            print_this("TEMP WARNING: DATA NOT FULLY SENT");
        }
        secondary_RM_access[i].V();
    }

    bool allAcksReceived = false;
    int curIndex = -1;
    int numAcksReceived = 0;
    print_this("Primary server preparing to wait for acks.");
    do {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        for (int i = 0; i < unprocessedAcks.size(); i++) {
            if (unprocessedAcks[i].timestamp == transaction.getTimestamp()) {
                if(unprocessedAcks[i].cnt == (*secondary_RM_sockets).size()) {
                    allAcksReceived = true;
                }
                print_this(std::to_string(unprocessedAcks[i].cnt));
            }
        }
    } while (allAcksReceived == false);

    return true;

}

void transactionManager::listenToSecondaryServers(bool* shutdownNotice, bool* RM_shutdownNotice) {

    int numSecondaryRMs = (*secondary_RM_sockets).size();
    struct pollfd pfd[numSecondaryRMs];
    for(int i = 0; i < numSecondaryRMs; i++) {
        pfd[i].fd = (*secondary_RM_sockets)[i].socketfd;
        pfd[i].events = POLLIN;
    }

    do {
        int num_events = poll(pfd, numSecondaryRMs, 15000);
        if(num_events > 0) {
            for (int i = 0; i < numSecondaryRMs; i++) {
                secondary_RM_access[i].P();
                uint64_t timestamp = 0;
                int bytes = recv((*secondary_RM_sockets)[i].socketfd, &timestamp, sizeof(timestamp), MSG_WAITALL);
                if (bytes == sizeof(timestamp)) {
                    if(timestamp != -1) {
                        print_this("Primary listener received an ack from secondary server " + std::to_string((*secondary_RM_sockets)[i].RM_id) +
                                   " with transaction timestamp " + std::to_string(timestamp));
                        bool acknowledged = false;

                        for(int j = 0; j < unprocessedAcks.size(); j++) {
                            if(unprocessedAcks[j].timestamp == timestamp) {
                                unprocessedAcks[j].cnt+= 1; acknowledged = true;
                                print_this("Ack acknowledged; Value " + std::to_string(unprocessedAcks[j].cnt) + " " + 
                                            std::to_string(j));
                            }
                        }
                        if (acknowledged == false) {
                            ackType ack; ack.cnt = 1; ack.timestamp = timestamp;
                            unprocessedAcks.push_back(ack);
                            print_this("Ack created");
                        }
                    }
                }
                else if (bytes == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) { /* do something*/}
                else {/* if no response was received, investigate if appropriate whether corresponding RM is still up */}
                secondary_RM_access[i].V();
            }
        }
    } while (*shutdownNotice == false && *RM_shutdownNotice == false);

}

void transactionManager::listenInSecondaryMode(bool* shutdownNotice, bool* RM_shutdownNotice) {

    int numSecondaryRMs = (*secondary_RM_sockets).size();
    struct pollfd pfd[numSecondaryRMs+1];
    pfd[0].fd = (*primary_RM_socket).socketfd;
    pfd[0].events = POLLIN;
    ElectionManager *election = new ElectionManager(selfSocket.RM_id,(*primary_RM_socket).RM_id);

    for(int i = 0; i < numSecondaryRMs; i++) {
        pfd[i+1].fd = (*secondary_RM_sockets)[i].socketfd;
        pfd[i+1].events = POLLIN;
    }
    bool operateInSecondaryMode = true;

    do {
        int num_events = poll(pfd, (*secondary_RM_sockets).size()+1, 20000);
        print_this("Secondary server" + std::to_string(selfSocket.RM_id) + "awoken");
        if(num_events > 0) {
            int i = 0;
            while (i < (*secondary_RM_sockets).size()+1) {
                if(pfd[i].revents != POLLIN) {print_this("connection not intercepted"); i++; continue;}
                if (i == 0) {
//                    print_this("Secondary RM has received an update from primary RM.");
                    transactionType transaction;
//                    print_this("Instantiated transaction type: " + std::to_string(transaction.getTransactionType()));
//                    print_this("Instantiated transaction user: " + std::to_string(transaction.getCurUserId()));
                    // print_this("connection intercepted");
                    int bytes = recv((*primary_RM_socket).socketfd, &transaction, sizeof(transaction), MSG_WAITALL);

                    if (bytes == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {continue;}
                    else if (bytes == 0) {
                        //Enter election mode.
                        //If win election, change operateInSecondaryMode to false.
                        print_this("Server " + std::to_string(selfSocket.RM_id) + " has detected primary breakdown.");
                        int resultElection = election->startNewElection(secondary_RM_sockets, primary_RM_socket);
                        if(resultElection==0){
                            operateInSecondaryMode=false;
                            (*primary_RM_socket) = selfSocket;
                            print_this("Server " + std::to_string(selfSocket.RM_id) + " has been elected as primary.");
                        }
                        else {
                            pfd[0].fd = (*primary_RM_socket).socketfd;
                            for (int j = 0; j < (*secondary_RM_sockets).size(); j++) {
                                pfd[j+1].fd = (*secondary_RM_sockets)[j].socketfd;
                            }
                            print_this("Server " + std::to_string(selfSocket.RM_id) + " has finished recognizing new primary.");
                        }
                        break;
                    }
                    else if (bytes < sizeof(transaction)) {
                        print_this("Data not fully received.");
                    }
                    else { i = (*secondary_RM_sockets).size()+1;}

                    int primary_curMaxSID = transaction.getCurMaxSID();
                    if((*cm_temp).getCurMaxSID() < primary_curMaxSID) (*cm_temp).setCurMaxSID(primary_curMaxSID);

                    int transactionType = transaction.getTransactionType();
                    bool operationSuccesful = false;

                    if(transactionType == 0) {
                        std::string targetUser(transaction.getTargetUsername());
                        print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " target of follow: " + targetUser);
                        operationSuccesful = (*db_temp).postFollow(targetUser, transaction.getCurUserId(), transaction.getTimestamp());
                    }
                    else if(transactionType == 1) {
                        std::string payload((transaction.getTweetData())._payload);
                        print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " update content: " + payload);
                        operationSuccesful = (*db_temp).postUpdate(transaction.getCurUserId(), transaction.getTweetData());
                    }
                    else if(transactionType == 2){
                        if (transaction.shouldTweetBeDuplicated()) {
                            (*cm_temp).registerConnection(transaction.getCurUserId(), transaction.getDuplicateSessionSID());
                            print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " duplicated tweets.");
                        }
                        else if((*cm_temp).doesClientHaveTwoConnections(transaction.getCurUserId())) {
                            if(transaction.shouldDuplicationTeardown()) {
                                int duplicateSID = (*cm_temp).getDuplicateSessionIndex(transaction.getCurUserId(),
                                                                                     transaction.getCurSessionSID());

                                (*cm_temp).closeConnection(transaction.getCurUserId(), duplicateSID);
                                print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " tore down duplicated connection.");
                            }
                        }

                        std::vector<tweetData> vec = (*db_temp).retrieveTweetsFromFollowed(transaction.getCurUserId(), transaction.getCurSessionSID());
                        print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " retrieved " + std::to_string(vec.size()) + " tweets.");
                        operationSuccesful = true;
                    }
                    else if(transactionType == 3) {
                        (*cm_temp).closeConnection(transaction.getCurUserId(), transaction.getCurSessionSID());
                        print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " registered disconnection.");
                        operationSuccesful = true;
                    }
                    else if (transactionType == 4) {
                        (*cm_temp).registerConnection(transaction.getCurUserId(), transaction.getCurSessionSID());
                        print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " registered connection.");
                        operationSuccesful = true;
                    }

                    if (operationSuccesful == true) {

                        print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " fully integrated transaction.");
                         uint64_t timestamp = transaction.getTimestamp();
                         int bytes = write((*primary_RM_socket).socketfd, &timestamp, sizeof(timestamp));
                         if (bytes == -1) {
                            print_this("PRIMARY SERVER UNAVAILABLE: " + i);
                         }
                         else if (bytes < sizeof(timestamp)) {
                            std::cout << "TEMP WARNING: DATA NOT FULLY SENT" << std::endl;
                        }
                        print_this("Secondary server " + std::to_string(selfSocket.RM_id) + " sent ack to primary.");
                    }

                     i++;
                }

                else {
                    //Enter election mode.
                    //If win election, change operateInSecondaryMode to false.
                    print_this("Server " + std::to_string(selfSocket.RM_id) + " has detected primary breakdown.");
                    int resultElection = election->startNewElection(secondary_RM_sockets, primary_RM_socket);
                    if(resultElection==0){
                        operateInSecondaryMode=false;
                        (*primary_RM_socket) = selfSocket;
                            print_this("Server " + std::to_string(selfSocket.RM_id) + " has been elected as primary.");
                    }
                    else {
                        pfd[0].fd = (*primary_RM_socket).socketfd;
                        for (int j = 0; j < (*secondary_RM_sockets).size(); j++) {
                            pfd[j+1].fd = (*secondary_RM_sockets)[j].socketfd;
                        }
                        print_this("Server " + std::to_string(selfSocket.RM_id) + " has finished recognizing new primary.");
                    }
                    break;
                }



            }
        }
        else {
            int temp = 0;
            print_this(std::to_string((*primary_RM_socket).socketfd));
            int bytes = recv((*primary_RM_socket).socketfd, &temp, sizeof(temp), MSG_WAITALL);
            if (bytes == 0) {
                //Enter election mode.
                //If win election, change operateInSecondaryMode to false.
                print_this("Server " + std::to_string(selfSocket.RM_id) + " has detected primary breakdown.");
                int resultElection = election->startNewElection(secondary_RM_sockets, primary_RM_socket);
                if(resultElection==0){
                    operateInSecondaryMode=false;
                    break;
                    //send announcement as new leader to all other servers
                }
                else {
                    pfd[0].fd = (*primary_RM_socket).socketfd;
                    for (int j = 0; j < (*secondary_RM_sockets).size(); j++) {
                        pfd[j+1].fd = (*secondary_RM_sockets)[j].socketfd;
                    }
                    print_this("Server " + std::to_string(selfSocket.RM_id) + " has finished recognizing new primary.");
                }
                continue;
            }
        }
    } while (*shutdownNotice == false && operateInSecondaryMode && *RM_shutdownNotice == false);
}

void transactionManager::configureServerInfo(std::vector<RM_info> *_secondary_server_sockets, RM_info *_primary_server_socket, RM_info _selfSocket) {
    this->secondary_RM_sockets = _secondary_server_sockets;
    this->primary_RM_socket = _primary_server_socket;
    this->selfSocket = _selfSocket;
    for(int i =0; i < (*secondary_RM_sockets).size(); i++) {
        this->secondary_RM_access.push_back(customBinarySemaphore());
    }
}

bool areThereNewTweets = false;
//! NOTE: The above boolean is being used, currently, in place of a check for pending tweets from the database.
//! Any use of it is simply placeholder and not currently functional

void handle_client_listener(bool* connectionShutdownNotice, std::mutex* outgoingQueueMUT,
                            std::vector<Message>* outgoingQueue, bool* outgoingQueueEmpty,
                            int* clientIndex, std::string* clientName, int SID,
                            databaseManager* db, connectionManager* cm, transactionManager* tm);

void handle_client_speaker(bool* connectionShutdownNotice, 
                            std::mutex* incomingQueueMUT, std::vector<Message>* incomingQueue, bool* incomingQueueEmpty,
                            std::mutex* outgoingQueueMUT, std::vector<Message>* outgoingQueue, bool* outgoingQueueEmpty,
                            int* clientIndex, std::string* clientName,
                            databaseManager* db, connectionManager* cm, transactionManager* tm);

std::vector<RM_location_info> RM_interaccess_locations;
std::vector<int> RM_outeraccess_ports;


//NOTE:: Below functions are placeholder and illustrative. It is possible that the handling of services may be shuffled among
//the existing functions, that new functions might be made or existing ones might be removed.

//This function serves to handle the functionality of the Client Connector / Socket manager for each client connection.
//When a connection is made by the Socket Headmaster / Server Connection Controller, this function will be passed onto the new thread.
void handle_client_connector(int socketfd, bool* serverShutdownNotice, bool *RM_shutdownNotice,
                             databaseManager* db, connectionManager* cm, transactionManager* tm)
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
    bool dataRead = false;
    int clientIndex = -1;
    int SID = rand();
    std::string clientName;

    std::thread listener(handle_client_listener, &clientShutdownNotice,
                        &outgoingMessagesMUT, &outgoingMessages, &outgoingQueueEmpty, &clientIndex, &clientName, SID,
                        db, cm, tm),

                speaker(handle_client_speaker, &clientShutdownNotice,
                            &incomingMessagesMUT, &incomingMessages, &incomingQueueEmpty,
                            &outgoingMessagesMUT, &outgoingMessages, &outgoingQueueEmpty, &clientIndex, &clientName,
                            db, cm, tm);


    while(clientShutdownNotice == false) {
        do {
            int num_events = poll(pfd, 1, 5000);
//            std::cout<< "data reading: " << ((num_events > 0) ? "succesfull " : "failed; repeating ") << std::endl << std::flush;
    //        std::this_thread::sleep_for(5000)
            bytes = recv(socketfd, &incomingPkt, sizeof(incomingPkt), MSG_WAITALL);
            if (bytes == -1 && errno == ECONNREFUSED) {
                //If there was an error reading from the socket, check if client is still connected by sending a dud ACK packet.
                bytes = write(socketfd, &incomingPkt, sizeof(incomingPkt));
                print_this("UNEXPECTED DISCONNECT FROM USER: " + clientName);
                if(bytes == -1) clientShutdownNotice = true;
            }
            else if(bytes == 0){
                print_this("ORDERLY DISCONNECT FROM USER: " + clientName);
                clientShutdownNotice = true;
            }
            else if (bytes < sizeof(incomingPkt))
                std::cout<< "TEMP WARNING: DATA NOT FULLY READ"<< std::endl << std::flush;
            else if (bytes == sizeof(incomingPkt)){
                dataRead = true;
                std::cout << "DATA RECEIVED CORRECTLY; PROCESSING" << std::endl << std::flush;
            }
        } while (*serverShutdownNotice == false && (bytes == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) &&
                 outgoingQueueEmpty == true && clientShutdownNotice == false && *RM_shutdownNotice == false);
            //! Additionally, check to see if there is anything in the outgoingBuffer

        //If there are messages to be sent to the client, send them first.

        std::unique_lock<std::mutex> lk_om(outgoingMessagesMUT);
        if(outgoingMessages.size() != 0) {
            int i = 0;
            bool sendBufferCond = true;
            while(i < outgoingMessages.size() && sendBufferCond) {

                 if(outgoingMessages[i].get_type() == 3) outgoingMessages[i].set_type(Type::UPDATE);
                 bytes = write(socketfd, &outgoingMessages[i], sizeof(outgoingMessages[i]));
                 if (bytes == -1) {
                    //If there was an error reading from the socket, check if client is still connected by sending a dud ACK packet.
                    print_this("UNEXPECTED DISCONNECT FROM USER: " + clientName);
                    if(bytes == -1) {clientShutdownNotice = true; sendBufferCond = false;}
                }

                else if (bytes < sizeof(incomingPkt))
                    std::cout << "TEMP WARNING: DATA NOT FULLY SENT PORRA PORRA CARALHO" << std::endl;
                else
                    std::cout << "Sent message: " << outgoingMessages[i].get_type() << " " <<outgoingMessages[i].get_payload() << std::endl <<std::flush;
                //! Make it get into a loop here until we are certain the buffer has put all data into buffer
                //! Make it wait for an ack for 20 seconds
                    //! Perhaps implement timeout measures for waiting for commands here
                //! Pretty sure we have to wait to check if we can write into the socket beforehand.

                i++;
            }
            if(sendBufferCond)
                std::cout <<"got to end of sending packets; packets sent: " << outgoingMessages.size() <<  std::endl << std::flush;
            outgoingMessages.clear();
            outgoingQueueEmpty = true;
        }
        lk_om.unlock();

        if(dataRead == false && *serverShutdownNotice == false) {
            continue;
        }

        if(*serverShutdownNotice == true || incomingPkt.get_type() == Type::SHUTDOWN_REQ) {
            //! Check to see how to write using nonblocking sockets. I'm 90% sure you can't just call write
            Message outgoingPkt;
            outgoingPkt.set_type( (*serverShutdownNotice) ? Type::SHUTDOWN_REQ : Type::ACK );
            clientShutdownNotice = true;
            //! Remember to set the rest of the fields here


            bytes = write(socketfd, &outgoingPkt, sizeof(outgoingPkt));
            //! Make it get into a loop here until we are certain the buffer has put all data into buffer
            //! Make it wait for an ack for 20 seconds
                //! Perhaps implement timeout measures for waiting for commands here

//            if(outgoingMessages.size() != 0) {
//                for(int i = 0; i < outgoingMessages.size(); i++) {
//
//                    //Attempt to send yet unsent messages to client
//                    //! If we implement 'messages we have attempted to send before' into the database,
//                    //! this part becomes unnecessary.
//
//                    //! Alternatively, we may just accept losing tweets if user signs off just before receiving the ones that
//                    //! were enqueued.
//
//                    //! This is already being done before this. Check things then delete this for loop.
//
//                }
//            }

        }
        else if(*RM_shutdownNotice == true) {
            clientShutdownNotice = true;
        }
        else if(incomingPkt.get_type() == Type::SIGN_IN) {

            bool operationSuccesful = true;

            clientIndex = (*db).getUserIndex(incomingPkt.get_payload());
            if(clientIndex == -1) {
                operationSuccesful = false;
            }

            if ((*cm).registerConnection(clientIndex, &SID) == false){
                operationSuccesful = false;
            }
            std::string SID_s = std::to_string(SID);

            Message ackPkt((operationSuccesful == true) ? Type::ACK : Type::NACK, SID_s.c_str());

            std::string userName(incomingPkt.get_payload());
            clientName = userName;

//            lk_om.lock();
//            outgoingMessages.push_back(ackPkt);
//            outgoingQueueEmpty = false;
//            lk_om.unlock();

            bytes = write(socketfd, &ackPkt, sizeof(ackPkt));
            if (bytes == -1) {
                //If there was an error reading from the socket, check if client is still connected by sending a dud ACK packet.
                print_this("UNEXPECTED DISCONNECT DURING SIGN_IN ATTEMPT FROM USER: " + clientName);
                clientShutdownNotice = true;

            }
            else {
                clientShutdownNotice = !operationSuccesful;
            }

            std::cout << "arrived at end of sign-in for user: "  << clientName << " with success " << operationSuccesful<< std::endl << std::flush;

            //Check list of connected users for [X]
            //If there is no [X] connected yet,
                //If [X] is in the database, register [X] as connected.
                //Save locally that current user is [X]
            //If [X] is connected to only one other device, mark one more connection.
                //Save locally that current user is [X]
            //If [X] already has two other connections, send rejection message to Client.


        }
        else if(incomingPkt.get_type() == Type::RECONNECT) {
            int tempSID = std::atoi(incomingPkt.get_payload());

            bool operationSuccesful = (*cm).registerReconnection(&clientIndex, tempSID);

            if(operationSuccesful) {
                SID = tempSID;
                clientName = (*db).getUserName(clientIndex);
            }
            std::string SID_s = std::to_string(SID);

            Message ackPkt((operationSuccesful == true) ? Type::ACK : Type::NACK, SID_s.c_str());

            bytes = write(socketfd, &ackPkt, sizeof(ackPkt));
            if (bytes == -1) {
                //If there was an error reading from the socket, check if client is still connected by sending a dud ACK packet.
                print_this("UNEXPECTED DISCONNECT DURING SIGN_IN ATTEMPT FROM USER: " + clientName);
                clientShutdownNotice = true;

            }
            else {
                clientShutdownNotice = !operationSuccesful;
            }

            std::cout << "arrived at end of reconnect for user: "  << clientName << " with success " << operationSuccesful<< std::endl << std::flush;

            
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

            std::cout << "User " << clientName << " received message: " <<incomingPkt.get_payload() << std::endl <<std::flush;

            incomingMessages.push_back(incomingPkt);
            incomingQueueEmpty = false;

            lk.unlock();
        }

        dataRead = false;
    }

    if(clientIndex != -1) (*cm).closeConnection(clientIndex, SID);
    listener.join();
    speaker.join();
    shutdown(socketfd, SHUT_RDWR);
    close(socketfd);
    print_this("Client " + clientName + " disconnect complete");

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
                            std::vector<Message>* outgoingQueue, bool* outgoingQueueEmpty,
                            int* clientIndex, std::string* clientName, int SID,
                            databaseManager* db, connectionManager* cm, transactionManager* tm)
{
    bool proceedCondition = false;
    bool stopOperation = false;
    std::unique_lock<std::mutex> lk(listenerProceedMUT);
    lk.unlock();
    while(stopOperation == false){
        lk.lock();
        while(proceedCondition == false) {
//            listenerPitstopCV.wait_for(lk, std::chrono::seconds(5), [cm_temp, clientIndex,SID]{return !cm_temp.doesClientHaveDuplicateTweets(*clientIndex, SID);});
//            print_this("First pitstop cleared");
            listenerPitstopCV.wait_for(lk, std::chrono::seconds(10), [clientIndex,SID, db]
                                        {return (*db).doesClientHavePendingTweets(*clientIndex,SID);});
                //! Update this check to use the database check for new tweets.
                //! Alternatively, keep this check but add an aditional one using database.
                //! Remember to update this check to check for tweets we haven't already attempted to send.
                // * Would it make sense to use areThereNewTweets? If the process is woken up forcefully,
                // there will always be new tweets, guaranteed.


            if ((*db).doesClientHavePendingTweets(*clientIndex, SID) == true){
                print_this("PINGES");
                proceedCondition = true;
            }
            else {
                print_this("It did not go through.");
            }
            if (*connectionShutdownNotice == true) {
                proceedCondition = true;
                std::cout << "listener shutdown ordered; shutting down" << std::endl << std::flush;
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

        print_this("Attempted to retrieve tweets");

        std::vector<tweetData> outTweets;
        (*tm).executeTransaction(*clientIndex, SID, &outTweets); //RetrieveTweetsFromFollowed

        std::cout << "Client " << *clientName << "has " << outTweets.size() <<" pending tweets." << std::endl << std::flush;

        std::unique_lock<std::mutex> lk_oq(*outgoingQueueMUT);
            //! Possibly make a local copy of outgoingQueue. Depending how long it takes to write onto database,
            //! might mean the listener holds onto the mutex for less time.
            //! Additionally, the queue shouldn't be too big at any one time.

        for(int i = 0; i < outTweets.size(); i++) {
            char userName[256];
            strcpy(userName, (*db).getUserName(outTweets[i].authorID).c_str());
            Message curPkt;
            curPkt.set_type(Type::UPDATE);
            curPkt.set_timestamp(outTweets[i].timestamp);
            curPkt.set_payload(outTweets[i]._payload);
            curPkt.set_author(userName);

            (*outgoingQueue).push_back(curPkt);

        }
        *outgoingQueueEmpty = false;
            //Put the tmeporary vector into the outgoingQueue


        lk_oq.unlock();
        proceedCondition = false;

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
                            std::mutex* outgoingQueueMUT, std::vector<Message>* outgoingQueue, bool* outgoingQueueEmpty,
                            int* clientIndex, std::string* clientName,
                            databaseManager* db, connectionManager* cm, transactionManager* tm)

{

    bool proceedSpeaker = false;
    bool stopOperation = false;
    while (*connectionShutdownNotice == false) {

        while(*incomingQueueEmpty && *connectionShutdownNotice == false) std::this_thread::sleep_for(std::chrono::seconds(5));

        std::unique_lock<std::mutex> lk(*incomingQueueMUT);

        for(int i = 0; i < incomingQueue->size(); i++ ) {
            Message curPkt = (*incomingQueue)[i];

//            std::cout << curPkt.get_payload() << std::endl << std::flush;

            if(curPkt.get_type() == Type::FOLLOW) {
                std::string targetUser(curPkt.get_payload());
                //Register client user as following targetUser using database access functions
                bool success = (*tm).executeTransaction(targetUser, *clientIndex, curPkt.get_timestamp());
                //PostFollow

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
                print_this("eecch");

                bool success = (*tm).executeTransaction(*clientIndex, newTweet);
                //postUpdate

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

//This function handles listening for connection requests at the server IP and SID.
//Created by main, will exist throughout the duration of the server.
void handle_connection_controller(bool* serverShutdownNotice, bool* RM_shutdownNotice, int serverID,
                                  databaseManager* db, connectionManager* cm, transactionManager* tm)
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
	serv_addr.sin_port = htons(RM_outeraccess_ports[serverID]);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	bzero(&(serv_addr.sin_zero), 8);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		std::cout << "ERROR on binding" << std::endl << std::flush;

	listen(sockfd, 15);

	clilen = sizeof(struct sockaddr_in);

	pfd[0].fd = sockfd;
	pfd[0].events = POLLIN;


	do {
        int num_events = poll(pfd, 1, 20000);

        print_this("Database save routine started.");
        (*db).saveDatabase();
        print_this("Database save routine finished.");

        std::cout<< "poll accept connection: " << ((num_events > 0) ? "succesful " : "no new connections; repeating ") << std::endl << std::flush;
        if(num_events > 0) {
            newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
            clientConnections.push_back(std::thread(handle_client_connector, newsockfd, serverShutdownNotice, RM_shutdownNotice,
                                                    db, cm, tm));
                //! Update this to also save the cli_addr and clilen into a vector.
        }
        else if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1);
//            printf("ERROR on accept");
	} while (*serverShutdownNotice == false && *RM_shutdownNotice == false);

	shutdown(sockfd, SHUT_RDWR);
	close(sockfd);
	std::cout <<"Server shutdown requested" << std::endl << std::flush;

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


void orderServerByID_ascending(std::vector<RM_info> *secondary_RM_sockets) {
    for(int i = 0; i < (*secondary_RM_sockets).size(); i++) {
        int indexOfCurLowest = i;
        int IDofCurLowest = (*secondary_RM_sockets)[i].RM_id;

        for (int j = i; j < (*secondary_RM_sockets).size(); j++) {
            if ((*secondary_RM_sockets)[j].RM_id < IDofCurLowest) {
                indexOfCurLowest = j;
                IDofCurLowest = (*secondary_RM_sockets)[j].RM_id;
            }
        }

        if(indexOfCurLowest != i) {
            std::iter_swap((*secondary_RM_sockets).begin() + i, (*secondary_RM_sockets).begin() + indexOfCurLowest);
        }
    }
}

void connectToFirstNServers(std::vector<RM_info> *secondary_RM_sockets, int n, int serverID) {
    int nServersConnected = 0;
    int i =0;
    do {
        if (i == serverID) {i++; continue;}
        int m_socket;
        struct sockaddr_in serv_addr;
        struct hostent *server;

//        print_this("Thread attempting SOMETHING.");

        server = gethostbyname(RM_interaccess_locations[i].hostname.c_str());
        if (server == NULL)
        {
            fprintf(stderr, "ERROR, no such host\n");
            std::cout << RM_interaccess_locations[i].hostname.c_str() << std::endl << std::flush;
            continue;
        }

        if ((m_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        {
            printf("ERROR opening socket\n");
            std::cout << RM_interaccess_locations[i].hostname.c_str() << " " << serverID << " " << errno  << " " << strerror(errno) << std::endl << std::flush;
            exit(0);
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(RM_interaccess_locations[i].port);
        serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
        bzero(&(serv_addr.sin_zero), 8);

        if (connect(m_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        {
            shutdown(m_socket, SHUT_RDWR);
            close(m_socket);
            continue;
        }
        print_this("Connection Established by client server " + std::to_string(serverID));

        write(m_socket, &serverID, sizeof(serverID));
        print_this("ID sent by client server " + std::to_string(serverID));

        fcntl(m_socket, F_SETFL, O_NONBLOCK);

        (*secondary_RM_sockets).push_back(RM_info(m_socket, i));
        nServersConnected++;


        i++;

    } while (nServersConnected < n && i < n);
}

void awaitNServerConnections(std::vector<RM_info> *secondary_RM_sockets, int n, int sockfd, int serverID){
    int nServersConnected = 0;
    while (nServersConnected < n) {
        print_this("Server " + std::to_string(serverID) +"listening for conenctions.");
        socklen_t clilen;
        struct sockaddr_in cli_addr;
        int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        print_this("Server " + std::to_string(serverID) +" accepted one connection; awaiting id.");

        int newsockid;
        int bytes = recv(newsockfd, &newsockid, sizeof(newsockid), 0);
        print_this("Server " + std::to_string(serverID) +" identified the connection: " + std::to_string(newsockid));

        fcntl(newsockfd, F_SETFL, O_NONBLOCK);

        (*secondary_RM_sockets).push_back(RM_info(newsockfd, newsockid));
        nServersConnected++;
    }
}

void monitorInterServerSockets(bool *serverShutdownNotice, bool *RM_shutdownNotice, transactionManager *local_tm) {

   local_tm->listenToSecondaryServers(serverShutdownNotice, RM_shutdownNotice);
}

void instantiate_server(bool* serverShutdownNotice, bool* RM_shutdownNotice, int serverID) {
    connectionManager local_cm;
    databaseManager local_db(&local_cm);
    local_db.loadDatabase();
    transactionManager local_tm(&local_db, &local_cm);
    local_tm.configureResources();

    std::vector<RM_info> secondary_RM_sockets;
    RM_info primary_RM_socket;
    RM_info selfSocket;
    selfSocket.RM_id = serverID;
//    int self_socket;

    print_this("Server " + std::to_string(serverID) + " starting up");

    int n;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    if ((selfSocket.socketfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        printf("ERROR opening socket");

    serv_addr.sin_family = AF_INET;
        //! Remember to update this to be over the internet
    serv_addr.sin_port = htons(RM_interaccess_locations[serverID].port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(serv_addr.sin_zero), 8);

    if (bind(selfSocket.socketfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        std::cout << "ERROR on binding on start-up // server " << serverID << std::endl << std::flush;

    listen(selfSocket.socketfd, 15);


    int nServersConnected = 0;


    if (serverID != 0) {
        int nServersToAwait = 0;
        int nServersToConnectTo = 0;

        if (serverID == 1) {
           nServersToAwait = 2;
           nServersToConnectTo = 1;
        }
        else if(serverID == 2) {
            nServersToAwait = 1;
            nServersToConnectTo = 2;
        }
        else if (serverID == 3) {
            nServersToAwait = 0;
            nServersToConnectTo = 3;
        }

        print_this("Server " + std::to_string(serverID) + " starting up");

        connectToFirstNServers(&secondary_RM_sockets, nServersToConnectTo, serverID);
        print_this("Server " + std::to_string(serverID) + " finished connecting.");
        awaitNServerConnections(&secondary_RM_sockets, nServersToAwait, selfSocket.socketfd, serverID);
        print_this("Server " + std::to_string(serverID) + " finished being connected to.");

        fcntl(selfSocket.socketfd, F_SETFL, O_NONBLOCK);

        orderServerByID_ascending(&secondary_RM_sockets);

        print_this("Server " + std::to_string(serverID) + ": " + std::to_string(secondary_RM_sockets[0].RM_id) + " " +
                    std::to_string(secondary_RM_sockets[1].RM_id) + " " + std::to_string(secondary_RM_sockets[2].RM_id));

        primary_RM_socket = secondary_RM_sockets[0];
        secondary_RM_sockets.erase(secondary_RM_sockets.begin());

        print_this("Server " + std::to_string(serverID) + ": " + std::to_string(secondary_RM_sockets[0].RM_id) + " " +
                    std::to_string(secondary_RM_sockets[1].RM_id) + " " + std::to_string(secondary_RM_sockets[2].RM_id));

        local_tm.configureServerInfo(&secondary_RM_sockets, &primary_RM_socket, selfSocket);
        print_this("Server " + std::to_string(serverID) + " configured. Entering listener mode.");
        local_tm.listenInSecondaryMode(serverShutdownNotice, RM_shutdownNotice);
        print_this("Primary function taken over by server " + std::to_string(serverID));
    }
    if(serverID == 0 || primary_RM_socket.RM_id == serverID) {
        primary_RM_socket.socketfd = selfSocket.socketfd;
        primary_RM_socket.RM_id = serverID;

        print_this("Server " + std::to_string(serverID) + " selected as primary; starting up.");

        if(serverID == 0) {
            awaitNServerConnections(&secondary_RM_sockets, 3, selfSocket.socketfd, serverID);
            print_this("Server " + std::to_string(serverID) + " finished being connected to.");

            orderServerByID_ascending(&secondary_RM_sockets);

            print_this("Server " + std::to_string(serverID) + ": " + std::to_string(secondary_RM_sockets[0].RM_id) + " " +
                        std::to_string(secondary_RM_sockets[1].RM_id) + " " + std::to_string(secondary_RM_sockets[2].RM_id));

            fcntl(selfSocket.socketfd, F_SETFL, O_NONBLOCK);

            local_tm.configureServerInfo(&secondary_RM_sockets, &primary_RM_socket, selfSocket);
        }

        
        print_this("Server " + std::to_string(serverID) + " configured. Spawning socket listener.");
        std::thread socket_controller(monitorInterServerSockets, serverShutdownNotice, RM_shutdownNotice, &local_tm);
        print_this("Server " + std::to_string(serverID) + " listener set up. Beginning operations.");
        handle_connection_controller(serverShutdownNotice, RM_shutdownNotice, serverID, &local_db, &local_cm, &local_tm);
        socket_controller.join();
    }

    shutdown(selfSocket.socketfd, SHUT_RDWR);
	close(selfSocket.socketfd);

    for(int i = 0; i < secondary_RM_sockets.size(); i++) {
        shutdown(secondary_RM_sockets[i].socketfd, SHUT_RDWR);
	    close(secondary_RM_sockets[i].socketfd);

    }
    print_this("Server " + std::to_string(serverID) + " has shut down.");
    //Open sockets to other RMs
}

int main(int argc, char **argv)
{

//    db_temp.addUser("@miku");
//    db_temp.addUser("@oblige");
//    db_temp.addUser("@noblesse");
//    db_temp.addUser("@miku2");
//    db_temp.postFollow("@oblige", db_temp.getUserIndex("@miku"), 0);
//    db_temp.postFollow("@oblige", db_temp.getUserIndex("@miku2"), 2);

    RM_interaccess_locations.push_back(RM_location_info("127.0.0.1", 4040));
    RM_interaccess_locations.push_back(RM_location_info("127.0.0.1", 4041));
    RM_interaccess_locations.push_back(RM_location_info("127.0.0.1", 4042));
    RM_interaccess_locations.push_back(RM_location_info("127.0.0.1", 4043));

    RM_outeraccess_ports.push_back(4002);
    RM_outeraccess_ports.push_back(4003);
    RM_outeraccess_ports.push_back(4004);
    RM_outeraccess_ports.push_back(4005);

    struct pollfd pfds[1];
    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;

    if(argc == 2)
        PORT = atoi(argv[1]);

    bool shutdownNotice = false;
    bool notice0 = false, notice1 = false, notice2 = false, notice3 = false;
    std::vector<bool*> RM_shutdownNotices = {&notice0, &notice1, &notice2, &notice3};

    std::thread RM0(instantiate_server, &shutdownNotice, RM_shutdownNotices[0], 0);
    std::thread RM1(instantiate_server, &shutdownNotice, RM_shutdownNotices[1], 1);
    std::thread RM2(instantiate_server, &shutdownNotice, RM_shutdownNotices[2], 2);
    std::thread RM3(instantiate_server, &shutdownNotice, RM_shutdownNotices[3], 3);

    int curLeader = 0;

    while(shutdownNotice == false) {
        // int num_events = poll(pfds, 1, 20000);
        // if(pfds[0].revents && POLLIN) {
            std::string temp;
            std::getline(std::cin, temp);
            if(temp == "s")
                shutdownNotice = true;
            else if (temp == "x") {
                *(RM_shutdownNotices[curLeader]) = true;
                curLeader++;
            }


        // }
        
        
    }

    RM0.join();
    RM1.join();
    RM2.join();
    RM3.join();

//    db_temp.addUser("miku");
//    db_temp.addUser("oblige");
//    db_temp.addUser("noblesse");
//
//    databaseManager manager;

//    manager.addUser("leo");
//    manager.addUser("Leah");
//
//    manager.loadListOfFollowers();
//    tweetData tweet;
//    tweet.authorID = 1;
//    tweet.numRecipientsRemaining = 1;
//    tweet.timestamp = 1;
//    tweet.tweetID = 0;
//    std::string a ="bingus";
//    strcpy(tweet._payload, a.c_str());
//    manager.postUpdate(1, tweet);
//
//    manager.saveListOfUsers();
//    manager.saveListOfFollowers();
//    manager.saveListOfReceivedTweets();
//    manager.saveListOfPendingTweets();
//    std::cout << manager.doesClientHavePendingTweets(0) << std::endl;

//    manager.loadDatabase();
//
//    std::cout << manager.retrieveTweetsFromFollowed(0)[0]._payload << std::endl;

//    std::cout << manager.getUserIndex("leo") << std::endl;;
//    std::cout << manager.getUserIndex("Leah") << std::endl;
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
    return 0;

}
