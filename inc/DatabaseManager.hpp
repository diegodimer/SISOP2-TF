#include <iostream>
#include <vector>
#include <mutex>
#include <inc/utils.hpp>

class databaseManager
{
public:
    std::vector<userType> listOfUsers;
    ;
    std::vector<std::vector<int>> listOfFollowers;
    std::vector<std::vector<tweetData>> listOfReceivedTweets;
    std::vector<std::vector<pendingTweet>> listOfPendingTweets;
    int numTweets;
    //All the above vectors are accessed using semaphores and reader-writer logic.
    //LOU is reader-preferred, LOF is reader preferred, LORT is reader preferred, LOPT is writer-preferred.

    customBinarySemaphore LOU_rw_sem;
    customBinarySemaphore LOF_rw_sem;
    customBinarySemaphore LORT_rw_sem;
    customBinarySemaphore LOPT_rw_sem;
    customBinarySemaphore LOPT_readTry;
    customBinarySemaphore LOPT_eraseTry;
    customBinarySemaphore tweetnum_sem;

    std::mutex LOU_cnt_mutex;
    std::mutex LOF_cnt_mutex;
    std::mutex LORT_cnt_mutex;
    std::mutex LOPT_w_cnt_mut;
    std::mutex LOPT_r_cnt_mut;

    databaseManager();
    databaseManager &operator=(databaseManager other)
    {
        listOfUsers = other.listOfUsers;
        listOfFollowers = other.listOfFollowers;
        listOfReceivedTweets = other.listOfReceivedTweets;
        listOfPendingTweets = other.listOfPendingTweets;
        numTweets = other.numTweets;
        return *this;
    }
    bool addUser(std::string name);
    int getUserIndex(std::string name);
    bool doesClientHavePendingTweets(int userID);
    bool postFollow(std::string targetUserName, int curUserID, time_t timestamp);
    bool postUpdate(int userID, tweetData tweet);
    std::vector<tweetData> retrieveTweetsFromFollowed(int userID);
    std::string getUserName(int userID);
    bool _alreadyFollowed(int targetUserID, int curUserID);
    bool _registerUpdate(int curUserID, tweetData tweet);
    bool _forwardUpdateToFollowers(int curUserID, uint64_t tweetID);
    std::vector<pendingTweet> _retrievePendingTweets(int userID);
    int _getNumFollowers(int curUserID);
    bool _clearUsersPendingTweets(int curUserID);
    bool _updateReceivedTweet(int targetUserID, uint64_t tweetID);
    bool _handleLateFollow(int targetUserID, int curUserID, time_t timestamp);

    int LOU_cnt = 0;
    int LOF_cnt = 0;
    int LORT_cnt = 0;
    int LOPT_r_cnt = 0;
    int LOPT_w_cnt = 0;
};
