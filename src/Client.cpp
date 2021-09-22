#include <iostream>
#include <inc/Client.hpp>
#include <cstdio>
#include <unistd.h>

Client::Client(){
  username = "Diego";
  mtx_sender.lock();
  mtx_receiver.lock();

};

Client::Client(std::string name)
{
  username = name;
};

void Client::client_controller()
{
  mtx_controller.lock();
  cout<<"Username is: " << username << endl;
  cout << "Sleep for 5s then unlock sender" << endl;
  sleep(5);
  mtx_sender.unlock();

};

void Client::client_sender(){
  mtx_sender.lock();
  cout << "Unlocked sender. Sleeping for 5s then unlocking receiver. " << endl;
  sleep(5);
  mtx_receiver.unlock();
};

void Client::client_receiver(){
  mtx_receiver.lock();
  cout << "Receiver unlocked." << endl;

};

// #define PORT 4000

// int main(int argc, char *argv[])
// {
//   int sockfd, n;
//   struct sockaddr_in serv_addr;
//   struct hostent *server;

//   char buffer[256];
//   if (argc < 2)
//   {
//     fprintf(stderr, "usage %s hostname\n", argv[0]);
//     exit(0);
//   }

//   server = gethostbyname(argv[1]);
//   if (server == NULL)
//   {
//     fprintf(stderr, "ERROR, no such host\n");
//     exit(0);
//   }

//   if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
//     printf("ERROR opening socket\n");

//   serv_addr.sin_family = AF_INET;
//   serv_addr.sin_port = htons(PORT);
//   serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
//   bzero(&(serv_addr.sin_zero), 8);

//   if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
//     printf("ERROR connecting\n");
//   while (true)
//   {
//     printf("Enter the message: ");
//     bzero(buffer, 256);
//     fgets(buffer, 256, stdin);

//     /* write in the socket */
//     n = write(sockfd, buffer, strlen(buffer));
//     if (n < 0)
//       printf("ERROR writing to socket\n");

//     bzero(buffer, 256);

//     /* read from the socket */
//     n = read(sockfd, buffer, 256);
//     if (n < 0)
//       printf("ERROR reading from socket\n");

//     printf("%s\n", buffer);
//   }
//   close(sockfd);

//   return 0;
// }