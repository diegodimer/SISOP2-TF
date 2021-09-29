#include <iostream>
#include <inc/Client.hpp>
#include <cstdio>
#include <unistd.h>
#include <poll.h>

Client::Client(char _username[], char _serveraddr[], int _port)
{
  strcpy(m_username, _username);
  m_socket = SocketClient(_serveraddr, _port);
  Message *signInMessage = new Message(Type::SIGN_IN, _username); // send username to server
  m_socket.send_message(*signInMessage);
  inboxHasItem = false;
  wait_server_response();
};

void Client::client_controller()
{
  struct pollfd pfds[2];

  std::string buff;
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

        if (!getline(std::cin, buff))
        { // got a crtl D from user (EOF);
          close_client();
        }
        fflush(stdin);
        client_sender(buff.c_str());
      }
      if (pfds[1].revents & POLLIN) // received message from socket
      {
        client_receiver();
      }
      if (pfds[1].revents & (POLLERR | POLLHUP))
      {
        // socket was closed
        cout << "Lost server connection." << endl
             << flush;
        close_client();
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
  cout << "serase" << endl;
};

void Client::client_sender(string command)
{
  string payload;
  SocketClient sckt = get_socket();

  if (command.compare("UPDATE") == 0)
  {
    cout << "What's happening? " << flush;
    while (payload.length() == 0) // workaround to get a payload message with body != 0
      getline(cin, payload);
    Message *msg = new Message(Type::UPDATE, payload.c_str());
    sckt.send_message(*msg);
  }
  else if (command.compare("FOLLOW") == 0)
  {
    cout << "Who you wanna follow? " << endl
         << flush;
    while (payload.length() == 0) // workaround to get a payload message with body != 0
      getline(cin, payload);
    Message *msg = new Message(Type::FOLLOW, payload.c_str());
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
    if (newMsg == NULL)
    {
      cout << "Got NULL message from server." << endl << flush;
      close_client();
    }
    else
    {
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
        close_client();
      default:
        add_message_to_inbox(newMsg);
        readOtherMessages = true;
        break;
      }
    }
  };

  if (readOtherMessages)
    inboxHasItem = true;
  return;
};

void Client::close_client()
{
  cout << "Bye!" << endl
       << flush;
  get_socket().close_connection();
  exit(0);
}