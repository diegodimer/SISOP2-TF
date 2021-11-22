#include <vector>
#include <inc/Message.hpp>
#include <iostream>
#include <inc/RM_info.hpp>
#include <unistd.h>

class ElectionManager
{

private:
    int serverID;
    int biggerID;
    int sendElectionPacket(int id, Message message);
    Message createElectionRequest();
    void sendAck(int port);

public:
    ElectionManager();
    ElectionManager(int thisServerID, int primaryServerID);
    int startNewElection(std::vector<RM_info> *secondary_RM_sockets);
    void updateBiggerID(int id);
    void addID(int id);
    void setBiggerID(int id);
    int getBiggerID();
    int getServerID();
    void setserverID(int id);
    std::vector<int> getListOfIDs();
};

ElectionManager::ElectionManager(int thisServer, int primaryServerID)
{
    serverID = thisServer;
    biggerID = primaryServerID;
}

void ElectionManager::sendAck(int port)
{
    Message ackMsg = Message();
    ackMsg.set_type(Type::ACK);
    write(port, &ackMsg, sizeof(ackMsg));
}
int ElectionManager::startNewElection(std::vector<RM_info> *secondary_RM_sockets)
{

    //Returns new primary server's ID
    //or -1 in case this server is to be the new Primary server

    // this->populateListOfIDs(secondary_RM_sockets);

    for (int i = 0; i < (*secondary_RM_sockets).size(); i++)
    {
        if ((*secondary_RM_sockets)[i].RM_id > this->serverID)
        {

            Message request = this->createElectionRequest();
            int response = sendElectionPacket((*secondary_RM_sockets)[i].socketfd, request);

            if (response != 0)
            {
                this->sendAck((*secondary_RM_sockets)[i].socketfd);
                this->biggerID = (*secondary_RM_sockets)[i].RM_id;
                return (*secondary_RM_sockets)[i].RM_id;
            }
        }
        else if ((*secondary_RM_sockets)[i].RM_id == this->serverID)
        {
            this->biggerID = this->serverID;
            return -1;
        }
    }
    return 0;
}

int ElectionManager::sendElectionPacket(int port, Message message)
{
    return write(port, &message, sizeof(message));
}

Message ElectionManager::createElectionRequest()
{
    Message req = Message();
    req.set_type(Type::ELECTION_REQ);
    return req;
}
