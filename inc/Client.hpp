#include <string>
#include <vector>
#include <inc/Message.hpp>
#include <mutex> 

/* Classe com as funções dos clientes. Ela vai ser adicionada no main_client e usada para
* gerenciar as threads dos clientes. Cada cliente terá três threads:
* controller: vai controlar qual das threads está executando e o que fazer
* sender: vai ser responsável pro enviar notificações ao servidor
* receiver: responsável por receber notificações do servidor
*/
using namespace std;
class Client {
    private:
        string username;
        vector<string> followers;
        vector<string> following;
        vector<Message> inbox;

        // mutex do usuário
        std::mutex mtx_controller;
        std::mutex mtx_receiver;
        std::mutex mtx_sender;
    public:
        Client();
        Client(std::string name); // this function should get everything from the database
        void client_controller();
        void client_sender();
        void client_receiver();
};