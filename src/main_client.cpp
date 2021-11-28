#include <thread>
#include <iostream>
#include <unistd.h>
#include <inc/Client.hpp>
#include <fstream>
#include <signal.h>

using namespace std;

std::mutex frontEndMutex;
std::condition_variable frontEndCondVar;
bool lookForServer;
SocketClient m_socket;
bool connected;
bool shutdown;
int client_front_end();

typedef struct
{
    string serveraddr;
    int port;
} st_Server;

Client *client;
int serverIndex;
bool firstTime;
vector<st_Server> myServerList;
string file_name;

int main(int argc, char **argv)
{
    if (argc != 3 || argv[1][0] != '@')
    {
        cout << "Wrong arguments on app call. Expected: @<username> <server list file>.\n";
        exit(0);
    };
    file_name = argv[2];
    firstTime = true;
    serverIndex = 0;
    lookForServer = true;

    std ::thread frontEndThread(client_front_end);
    client = new Client();
    client->set_username(argv[1]);

    client->client_controller();
    frontEndThread.join();

    printf("Finishing main_client\n");
}

int client_front_end()
{
    while (true)
    {
        {
            std::unique_lock<std::mutex> lock(frontEndMutex);
            frontEndCondVar.wait_for(lock, std::chrono::seconds(1000), []()
                                     { return lookForServer; });
            connected = false;
            if (client->isFirstConnect())
            {
                ifstream file(file_name);
                string line;
                while (getline(file, line))
                {
                    size_t pos = line.find(",");
                    if (string::npos != pos)
                    {
                        st_Server serverEntry;
                        serverEntry.serveraddr = line.substr(0, pos);
                        serverEntry.port = atoi(line.substr(pos + 1, line.length()).c_str());
                        myServerList.push_back(serverEntry);
                    }
                }
                for (int i = 0; i < myServerList.size(); i++)
                {
                    if (client->sign_in(client->get_username(), (char *)myServerList[i].serveraddr.c_str(), (int)myServerList[i].port) == 0)
                    {
                        connected = true;
                        serverIndex = i;
                        break;
                    }
                }
                client->firstConnectComplete();
            }
            else
            {
                auto start = std::chrono::system_clock::now();
                int i = 0;
                while (true)
                {
                    if (i == myServerList.size())
                        i = 0;
                    if (client->sign_in(client->get_username(), (char *)myServerList[i].serveraddr.c_str(), (int)myServerList[i].port) == 0)
                    {
                        connected = true;
                        serverIndex = i;
                        break;
                    }
                    auto end = std::chrono::system_clock::now();
                    std::chrono::duration<double> elapsed_seconds = end - start;
                    if (elapsed_seconds.count() > 30)
                    {
                        shutdown = 1;
                        cout << "Connection has been lost to all servers. After 30s, client is giving up on retrying.\nShutting down." << endl
                             << flush;
                        break;
                    }
                    i++;
                }
            }
            if (shutdown == 1)
                abort();
            if (connected)
            {
                lookForServer = false;
                frontEndCondVar.notify_one();
            }
            else
            {
                cout << "I'm sorry but I couldn't find any server to connect. Please review your serverList.txt file and try again." << endl
                     << flush;
                lookForServer = false;
                frontEndCondVar.notify_one();
                exit(0);
            }
        }
    }
    return 0;
}