#include <inc/DatabaseManager.hpp>
#include <iostream>

databaseManager::databaseManager(){

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

bool databaseManager::doesClientHavePendingTweets(int userID) {

    //WRiters-preferred read operation
    this->LOPT_readTry.P();
    std::unique_lock<std::mutex> lk_r_m(this->LOPT_r_cnt_mut);
    this->LOPT_r_cnt++;
    if(this->LOPT_r_cnt == 1) this->LOPT_rw_sem.P();
    lk_r_m.unlock();
    this->LOPT_readTry.V();

    bool answer = this->listOfPendingTweets[userID].size() != 0;
//    std::cout << "Checked whether client has pending tweets " << this->listOfPendingTweets[userID].size()  << std::endl << std::flush;

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
    std::cout<< "Followers: " << this->listOfFollowers[curUserID].size() << std::endl << std::flush;

    lk.lock();
    this->LOF_cnt--;
    if(this->LOF_cnt == 0) this->LOF_rw_sem.V();
    lk.unlock();

    return numberOfFollowers;
}

bool databaseManager::postFollow(std::string targetUserName, int curUserID, time_t timestamp) {
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
        std::cout << targetUserID << std::endl << std::flush;
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

    //Complete the tweet metadata with the number of followers this user has.
    this->tweetnum_sem.P();
    tweet.tweetID = this->numTweets;
    numTweets++;
    this->tweetnum_sem.V();

    if(this->_getNumFollowers(userID) != 0) {

        //Put the tweet in the list of received tweets
        if(!this->_registerUpdate(userID, tweet)) return false;

        //For each user in said list, add pendingTweet regarding current tweet
        if(!this->_forwardUpdateToFollowers(userID, tweet.tweetID)) return false;
            //! weh. Make a function to remove the update if it fails to add it.

    }

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
    this->listOfPendingTweets[userID].clear();

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

std::vector<tweetData> databaseManager::retrieveTweetsFromFollowed(int userID){

    std::vector<tweetData> receivedTweets;
    std::vector<pendingTweet> pendingTweets = this->_retrievePendingTweets(userID);
//    this->_clearUsersPendingTweets(userID);

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

    for(int i = 0; i < pendingTweets.size(); i++) {
        this->_updateReceivedTweet(pendingTweets[i].userAuthor, pendingTweets[i].tweetID);
    }

    return receivedTweets;

}
