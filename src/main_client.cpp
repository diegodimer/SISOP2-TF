#include "../inc/Client.hpp"
#include "../inc/Message.hpp"



Client::Client(std::string user, int serverPort, std::string serverAddress){

    this->user=user;
    this->serverPort=serverPort;
    this->serverAddress=serverAddress;
    this->connectToServer();

    std::unique_lock<std::mutex> lockPrint(this->inputMUTEX);
}


//i gotta have a control thread for this dude
//note to self: understand how the controller is supposed to work



//this dude receives stuff
//puts it on incoming queue to be freed later
static void *Client::receiverThreadExecutor(void* arg){
    Client *client = (Client*) arg;
    Message *message;

    while(true){
      //  message = get message from socket that diego is doing

      if(message == NULL){
          exit(1);
      }

      std::unique_lock<std::mutex> lockPrint(client->printMUTEX);
      //should i lock control? future me will know
      if(message->getType()==NEW_TWEET || message->getType()==NEW_FOLLOW ){
        client->incomingMessages.push_back(message);
      }
      else if(message->getType()==CONTROL_MSG){
          //???? it cries?
      }
      
     lockPrint.unlock(); 
    }
}

//this thread will check the user's incomingQueue, 
//if there is some new message to be displayed, and print mutex is unlocked, locks it
//and prints all the stuff that came while user was typing some dank memes.
static void *Client::displayThreadExecutor(void* arg){
//get dude's incoming queue
//

}

// This dude sends stuff
//needs sockets for me to get what i must do here
// should I even have an outgoing queue for this? is it necessary? (check with diana)
void *Client::senderThreadExecutor(void* arg){
    Client *client = (Client*) arg;
    std::string input;

    while(true){

        // THINGS GET CRITICAL
        // lock any mutex 
        // probably clean user buffer
        // locks print, locks input, 
        std::unique_lock<std::mutex> lockController(client->controlMUTEX);
        std::unique_lock<std::mutex> lockPrint(client->printMUTEX);
        std::unique_lock<std::mutex> lockInput(client->inputMUTEX);

        input = getUserCommand();

        if(input.compare(SEND_COMMAND)==0){
            client->sendTweet();
        }
        else if(input.compare(FOLLOW_COMMAND)==0){
            client->sendFollowRequest();
        }
        else{
            std::cout << "Command not recognized!" << std::endl;
        }

       lockInput.unlock();
       lockPrint.unlock();
       lockController.unlock();

    }
}

std::string getUserCommand(){
    std::string input = "";
    char c;

    std::cout << "What's the next action you'd like to do?" << std::endl;

    do{
        c = getchar();
        input+=c;
    }while( c!=' ' && c!= END_OF_INPUT );

    input.pop_back();

    return input;
}

void Client::sendTweet(){
    //TO DO
}

void Client::sendFollowRequest(){
    //TO DO
}

void Client::displayNewTweet(Message *message){
  //  std::cout << "New Tweet from" << message.getAuthor << std::endl;    <====NEED AUTHOR?
  std::cout << message->getBody() << "\n\n\n";
  message = NULL;
}

void Client::displayNewFollow(Message *message){
   // std::cout << "You have a new folllower: " << message.getAuthor() << "\n\n\n" << std::endl;   
}



