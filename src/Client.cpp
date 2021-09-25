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
