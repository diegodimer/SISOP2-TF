#include <iostream>
#include <inc/Client.hpp>
#include <cstdio>
#include <unistd.h>
#include <poll.h>

extern std::mutex frontEndMutex;
extern std::condition_variable frontEndCondVar;
extern bool lookForServer;
extern SocketClient m_socket;
extern bool connected;
extern bool shutdown;
Client::Client()
{
}

int Client::sign_in(char _username[], char _serveraddr[], int _port, bool firstConnect)
{
  if (firstConnect)
  {
    strcpy(m_username, _username);
    m_socket = SocketClient(_serveraddr, _port);
  }
  else
  {
    m_socket.set_port(_port);
    m_socket.set_hostname(_serveraddr);
  }

  if (m_socket.connect_to_server() != 0)
    return -1;

  Message *signInMessage;

  if (firstConnect)
    signInMessage = new Message(Type::SIGN_IN, _username); // send username to server
  else
    signInMessage = new Message(Type::RECONNECT, to_string(this->get_uid())); // send username to server

  if (m_socket.send_message_no_retry(*signInMessage) < 0)
    return -1;

  inboxHasItem = false;
  return wait_server_response();
};

void Client::client_controller()
{
  struct pollfd pfds[2];

  std::string buff;
  Message *msg = new Message();

  pfds[0].fd = STDIN_FILENO;
  pfds[0].events = POLLIN;

  { // waits first server connection
    std::unique_lock<std::mutex> lock(frontEndMutex);
    frontEndCondVar.wait_for(lock, std::chrono::seconds(1000), []()
                             { return !lookForServer; });
  }

  pfds[1].events = POLLIN;
  int i = 0;
  shutdown = false;
  
  if (!connected)
  {
    exit(0);
  }

  auto start = std::chrono::system_clock::now();

  while (1)
  {
    pfds[1].fd = get_socket_num();
    if (poll(pfds, 2, 100) != -1)
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
      else if (pfds[1].revents & POLLIN) // received message from socket
      {
        client_receiver();
      }
      else if (pfds[1].revents & (POLLERR | POLLHUP))
      {
        // socket was closed
        cout << "Lost server connection." << endl
             << flush;
        { // reestablish server connnection
          get_socket().close_connection();
          lookForServer = true;
          frontEndCondVar.notify_one();
          std::unique_lock<std::mutex> lock(frontEndMutex);
          frontEndCondVar.wait_for(lock, std::chrono::seconds(1000), []()
                                   { return !lookForServer; });
        }
      }
      auto end = std::chrono::system_clock::now();
      std::chrono::duration<double> elapsed_seconds = end - start;
      if (elapsed_seconds.count() > 7)
      {
        //cout << "Checking if server is alive." << endl << flush;
        check_server_liveness();
        start = end;
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

    while (sckt.send_message(*msg) != 0)
    {
      {
        sckt.close_connection();
        lookForServer = true;
        frontEndCondVar.notify_one();
        std::unique_lock<std::mutex> lock(frontEndMutex);
        frontEndCondVar.wait_for(lock, std::chrono::seconds(1000), []()
                                 { return !lookForServer; });
      }
    }
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
  if (msg->get_type() == Type::SHUTDOWN_REQ)
  {
    cout << "Socket closed by server. Closing and exiting." << endl
         << flush;
    close_client();
  }
  else
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

int Client::wait_server_response()
{
  Message *newMsg;
  SocketClient sckt = get_socket();
  bool confirmationReceived = false;
  bool readOtherMessages = false;

  while (!confirmationReceived)
  {
    newMsg = sckt.receive_message_no_retry();
    if (newMsg == NULL)
    {
      cout << "Got NULL message from server." << endl
           << flush;
      close_client();
      return -1;
    }
    else
    {
      switch (newMsg->get_type())
      {
      case Type::ACK:
        cout << "Connected succesfully." << endl
             << flush;
        set_uid(atoi(newMsg->get_payload()));
        free(newMsg);
        confirmationReceived = true;
        break;
      case Type::NACK:
        cout << "Connection refused by server." << endl
             << flush;
        free(newMsg);
        close_client();
      case Type::SHUTDOWN_REQ:
        cout << "Server closed." << endl
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

  return 0;
};

void Client::close_client()
{
  cout << "Bye!" << endl
       << flush;
  get_socket().close_connection();
  shutdown = true;
  exit(0);
}

void Client::check_server_liveness()
{
  Message *msg = new Message(Type::KEEP_ALIVE, get_username());

  while (get_socket().send_message(*msg) != 0)
  {
    {
      get_socket().close_connection();
      lookForServer = true;
      frontEndCondVar.notify_one();
      std::unique_lock<std::mutex> lock(frontEndMutex);
      frontEndCondVar.wait_for(lock, std::chrono::seconds(1000), []()
                               { return !lookForServer; });
    }
  }
}