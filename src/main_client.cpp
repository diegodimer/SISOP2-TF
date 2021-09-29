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

    Client *client = new Client(argv[1], argv[2], atoi(argv[3]));

    client->client_controller();

    printf("Finishing main_client\n");
}