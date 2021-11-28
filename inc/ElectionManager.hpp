#include <vector>
#include <inc/Message.hpp>
#include <iostream>
#include <inc/RM_info.hpp>
#include <unistd.h>

void print_this(std::string s) {
    std::cout << s << std::endl << std::flush;
}

class ElectionManager
{

private:
    int serverID;
    int biggerID;
    int sendElectionPacket(int id, Message message);
    Message createElectionRequest();
    int sendAck(int port);

public:
    ElectionManager();
    ElectionManager(int thisServerID, int primaryServerID);
    int startNewElection(std::vector<RM_info> *secondary_RM_sockets, RM_info *primary_RM_socket);
    void updateBiggerID(int id);
    void addID(int id);
    void setBiggerID(int id);
    bool waitForResponse(RM_info *newPrimaryCandidate);
    bool waitForRequest(RM_info *secondaryServer);
    void removeDownedServer(std::vector<RM_info> *secondary_RM_sockets, int serverIndex);
    void removeServerFromList(std::vector<RM_info> *secondary_RM_sockets, int serverIndex);
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

int ElectionManager::sendAck(int port)
{
    Message ackMsg = Message();
    ackMsg.set_type(Type::ACK);
    return write(port, &ackMsg, sizeof(ackMsg));

}

bool ElectionManager::waitForResponse(RM_info *newPrimaryCandidate) {
    bool responseReceived = false;
    int bytesReceived = 0;
    Message answer;

    while((bytesReceived = recv(newPrimaryCandidate->socketfd, &answer, sizeof(answer), MSG_WAITALL)) != sizeof(answer)) {
        if(bytesReceived == 0) return false;
    }

    return true;
}

bool ElectionManager::waitForRequest(RM_info *secondaryServer) {
    return waitForResponse(secondaryServer);
}

void ElectionManager::removeDownedServer(std::vector<RM_info> *secondary_RM_sockets, int serverIndex) {
    (*secondary_RM_sockets).erase((*secondary_RM_sockets).begin() + serverIndex);
}

void ElectionManager::removeServerFromList(std::vector<RM_info> *secondary_RM_sockets, int serverIndex) {
    removeDownedServer(secondary_RM_sockets, serverIndex);
}

int ElectionManager::startNewElection(std::vector<RM_info> *secondary_RM_sockets, RM_info *primary_RM_socket)
{

    //Returns -1 if current server is not the new primary server
    //or 0 in case this server is to be the new Primary server

    //Function will change secondary_RM_sockets and primary_RM_socket, so for the secondary servers
    //the result of the election will automatically change their contents.
    //Function will also detect and remove any downed servers it detects during any point of the election process.

    //Returning 0 or -1 helps transition the program straight into primary server mode with a simple check while inside
    //listenInSecondaryMode.

    //As every single secondary server will take exclusive control over the sockets and there will be no parallelism
    //present inside of each secondary server, semaphores are not required to arbitrate the reading and writing into sockets.

   

    //Go through all servers that have higher ID than current server. secondary_RM_sockets is already an ordered vector.
    //If there are no active servers with higher ID, server will iterate through vector without doing anything. ResponseReceived will be 0.
    //If there are active servers with higher ID than this server, but all of them fail during election, 
        //server will progressively remove from the list of active RMs those that have broken TCP connections, indicating they are downed.
        //it will arrive at the end of the loop with responseReceived = 0, and become the primary.
    //If there are active servers with higher ID and they do not fail,
        //server will update its secondary_RM_sockets and primary_RM_sockets to reflect the new state of the server structure.
    int i = 0;
    bool responseReceived = false;
    print_this("Server " + std::to_string(serverID) + " has entered election mode.");
    while(i < (*secondary_RM_sockets).size() && responseReceived == false) {
        if((*secondary_RM_sockets)[i].RM_id < this->serverID) {
            Message request = this->createElectionRequest();
            int bytesSent = sendElectionPacket((*secondary_RM_sockets)[i].socketfd, request);

            if(bytesSent == -1 && (errno == ENOTCONN || errno == ECONNRESET || errno == EPIPE)) {
                removeDownedServer(secondary_RM_sockets, i);
                continue;
            }
            else if(bytesSent < sizeof(request)) continue;  //Attempt sending election packet again. sendElectionPacket does not block until sent or server downed. 

            print_this("Server " + std::to_string(serverID) + " has sent election packet to best candidate, server " +
                                                  std::to_string((*secondary_RM_sockets)[i].RM_id) + ".");

            RM_info primaryServerCandidate = (*secondary_RM_sockets)[i];
            bool serverNotDowned = waitForResponse(&primaryServerCandidate);

            if (serverNotDowned == false) {    //Server could've been downed between receiving this server's election req and answering it.
                removeDownedServer(secondary_RM_sockets, i);
                continue;
            }
            else {
                print_this("Server " + std::to_string(serverID) + " has received a response from best candidate.");
                responseReceived = true;
                (*primary_RM_socket) = (*secondary_RM_sockets)[i];
                removeServerFromList(secondary_RM_sockets, i);
            }
        }

        i++;
    }

    if(responseReceived) return -1; //Return that the current server is not the new primary server.

    print_this("Server " + std::to_string(serverID) + " has noticed it is the highest ID server.");

    //If function arrived here, then there are no other active servers with better/lower index than current serverID.
    //If that is the case, then other servers have certainly already sent election requests to this server, or are about to.
    //Read all the requests then answer them to confirm current server as the next primary server.

    //Read all election_req from active servers, guaranteeing that all currently active servers have entered election mode
    i = 0;

    while(i < (*secondary_RM_sockets).size()) {
        RM_info curSecondaryServer = (*secondary_RM_sockets)[i];
        bool serverNotDowned = waitForRequest(&curSecondaryServer);

        print_this("Server " + std::to_string(serverID) + " has received one response.");

        if(serverNotDowned == false) {
            removeDownedServer(secondary_RM_sockets, i);
            continue;
        }

        i++;
    }

    print_this("Server " + std::to_string(serverID) + " has finished receiving election requests.");

    //Confirm current server as next primary server by acking all received election requests.
    //There is no need to maintain state about which server has already sent election requests.
    //If they are still up, and the function has arrived at this point, then all still-online servers have already sent their requests.
    bool responsesSent = false;
    i = 0;

    while(i < (*secondary_RM_sockets).size() && responsesSent == false) {
        int bytesSent = sendAck((*secondary_RM_sockets)[i].socketfd);
        if(bytesSent == -1 && (errno == ENOTCONN || errno == ECONNRESET || errno == EPIPE)) {
            removeDownedServer(secondary_RM_sockets, i);
            continue;
        }
        else if(bytesSent < sizeof(Message)) continue; //Else try sending it again. sendAck can fail to send, it does not attempt until succesful or server downed.
        i++;
    }

    print_this("Server " + std::to_string(serverID) + " has finished receiving acking election requests.");

    return 0;   //Return that current server is the new primary server.
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
