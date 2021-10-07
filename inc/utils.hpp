#include <condition_variable>

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