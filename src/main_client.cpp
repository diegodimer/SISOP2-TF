#include <thread>
#include <iostream>
#include <inc/Client.hpp>

using namespace std;

int main(int argc, char **argv)
{


    if (argc != 4 || argv[1][0] != '@')
    {
        cout << "Wrong arguments on app call. Expected: @<username> <server> <port>.\n";
        exit(0);
    };

    Client *client = new Client();

    thread controller(&Client::client_controller, client);
    thread sender(&Client::client_sender, client);
    thread receiver(&Client::client_receiver, client);    
    controller.join();
    sender.join();
    receiver.join();
    printf("Finishing main_client\n");
}