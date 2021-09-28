#include <iostream>
#include <inc/Client.hpp>
#include <cstdio>
#include <unistd.h>
#include <poll.h>

Client::Client(char _username[], char _serveraddr[], int _port)
{
  strcpy(m_username, _username);
  m_socket = SocketClient(_serveraddr, _port);
  inboxHasItem = false;
  wait_server_response();
};

void Client::client_controller()
{
  struct pollfd pfds[2];

  char buff[256];
  Message *msg = new Message();

  pfds[0].fd = STDIN_FILENO;
  pfds[0].events = POLLIN;

  pfds[1].fd = get_socket_num();
  pfds[1].events = POLLIN;

  uint16_t seq = 0;
  while (1)
  {
    if (poll(pfds, 2, 100 != -1))
    {
      if (pfds[0].revents & POLLIN) // message from stdin
      {
        cin >> buff;
        fflush(stdin);
        client_sender(buff);
      }
      if (pfds[1].revents & POLLIN) // received message from socket
      {
        client_receiver();
      }
      if (pfds[1].revents & (POLLERR | POLLHUP))
      {
        // socket was closed
        cout << "oh no" << endl
             << flush;
      }
    }
    else
    {
      if (inboxHasItem)
      { // these are the messages server sent before ACK on connection
        for (const auto &it : get_inbox())
          print_message(it);
        inboxHasItem = false;
      }
    }
  }
};

void Client::client_sender(char command[])
{
  string payload;
  SocketClient sckt = get_socket();

  if (strcmp(command, "UPDATE") == 0)
  {
    cout << "What's happening? " << flush;
    while (payload.length() == 0) // workaround to get a payload message with body != 0
      getline(cin, payload);
    Message *msg = new Message(Type::UPDATE, 1, 256, payload.c_str());
    sckt.send_message(*msg);
  }
  else if (strcmp(command, "FOLLOW") == 0)
  {
    cout << "Who you wanna follow? " << endl
         << flush;
    while (payload.length() == 0) // workaround to get a payload message with body != 0
      getline(cin, payload);
    Message *msg = new Message(Type::FOLLOW, 1, 256, payload.c_str());
    sckt.send_message(*msg);
  }
  else
  {
    cout << "Invalid command. Options are: UPDATE and FOLLOW" << endl
         << flush;
  }
};

void Client::client_receiver()
{
  Message *msg = m_socket.receive_message();
  print_message(msg);
};

void Client::print_message(Message *msg)
{
  Type messageType = msg->get_type();
  if (messageType == Type::UPDATE)
  {
    cout << "FROM " << msg->get_author() << " IN " << msg->get_timestamp_string() << msg->get_payload() << endl
         << endl
         << flush;
  }
  else if (messageType == Type::NACK)
  {
    cout << "ERROR " << msg->get_payload() << endl
         << endl
         << flush;
  }
}

void Client::wait_server_response()
{
  Message *newMsg;
  SocketClient sckt = get_socket();
  bool confirmationReceived = false;
  bool readOtherMessages = false;

  while (!confirmationReceived)
  {
    newMsg = sckt.receive_message();
    switch (newMsg->get_type())
    {
    case Type::ACK:
      cout << "Connected succesfully." << endl
           << flush;
      free(newMsg);
      confirmationReceived = true;
      break;
    case Type::NACK:
      cout << "Connection refused by server." << endl
           << flush;
      free(newMsg);
      confirmationReceived = true;
    default:
      add_message_to_inbox(newMsg);
      readOtherMessages = true;
      break;
    }
  };

  if (readOtherMessages)
    inboxHasItem = true;
  return;
};
