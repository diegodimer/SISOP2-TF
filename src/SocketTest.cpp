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
#define PORT 4060
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

  Message message(Type::ACK, 1, 256, "");
  n = write(newsockfd, &message, sizeof(Message));
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
      printf("received a message: %s\n", messagerec->get_payload());
    }
    string payload;
    printf("Enter your response: ");
    cin >> payload;
    Message message(Type::UPDATE, 1, 256, payload);
    message.set_author("diego");
    n = write(newsockfd, &message, sizeof(Message));

    sleep(5);
    message.set_type(Type::ACK);
    n = write(newsockfd, &message, sizeof(Message));

    sleep(2);
    message.set_type(Type::NACK);
    n = write(newsockfd, &message, sizeof(Message));

    message.set_type(Type::UPDATE);
    n = write(newsockfd, &message, sizeof(Message));

    if (n < 0)
      printf("ERROR writing to socket");
    cout << "Wrote to socket successfully." << endl;
  }
}