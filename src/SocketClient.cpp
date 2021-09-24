// boilerplate code to test socket communication
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <inc/Message.hpp>
#include <inc/Socket.hpp>
#include <thread>
#define PORT 4050
#include <iostream>
using namespace std;

int main()
{
  int sockfd, newsockfd, n;
  socklen_t clilen;
  char buffer[256];
  struct sockaddr_in serv_addr, cli_addr;

  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    printf("ERROR opening socket");

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  bzero(&(serv_addr.sin_zero), 8);

  if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    printf("ERROR on binding");

  listen(sockfd, 5);

  clilen = sizeof(struct sockaddr_in);
  if ((newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen)) == -1)
    printf("ERROR on accept");

  bzero(buffer, 256);

  while (1)
  {

    Message *messagerec = new Message();
    memset(messagerec, 0, sizeof (Message));
    int n = read(newsockfd, messagerec, sizeof(Message));
    if (n < 0)
    {
      // printf("ERROR reading from socket\n");
      return NULL;
    } else {
      printf("received a message: %s\n", messagerec->get_body());
    }
    string author;
    string body;
    printf("Enter your response: ");
    cin >> author;
    cin >> body;
    Message message(Type::NEW_TWEET, body, author);
    n = write(newsockfd, &message, sizeof(Message));
    if (n < 0)
      printf("ERROR writing to socket");
    cout << "Wrote to socket successfully." << endl;
  }
}