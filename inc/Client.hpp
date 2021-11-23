#include <string>
#include <vector>
#include <inc/Message.hpp>
#include <inc/Socket.hpp>
#include <mutex> 
#include <condition_variable>
#include <chrono>

extern std::mutex frontEndMutex;
extern std::condition_variable frontEndCondVar;
extern bool lookForServer;
extern SocketClient m_socket;
extern bool connected;

/* Classe com as funções dos clientes. Ela vai ser adicionada no main_client e usada para
* gerenciar as threads dos clientes. Cada cliente terá três threads:
* controller: vai controlar qual das threads está executando e o que fazer
* sender: vai ser responsável pro enviar notificações ao servidor
* receiver: responsável por receber notificações do servidor
*/
using namespace std;
class Client {
    private:
        char m_username[256];
        vector<string> m_followers;
        vector<string> m_following; // is this information relevant?
        vector<Message*> m_inbox;
        uint64_t uId;

        bool inboxHasItem;


    public:

        Client();
        int sign_in(char _username[], char _serveraddr[], int _port, bool);
        void client_controller();
        void client_sender(std::string command);
        void client_receiver();
        void print_message(Message *msg);

        char* get_username() { return m_username; };
        void set_username(char username[]) { strcpy(m_username, username); };

        vector<string> get_followers() { return m_followers; }
        void set_followers( vector<string>  followers) { m_followers = followers; }
        void add_follower(string  follower) { m_followers.push_back(follower); }

        vector<Message*> get_inbox() { return m_inbox; } 
        void set_inbox(vector<Message*> inbox) { m_inbox = inbox; } 
        void add_message_to_inbox(Message *message) { m_inbox.push_back(message); }
        void clear_inbox() { m_inbox.clear(); }

        int get_socket_num() { return m_socket.get_socket(); }
        SocketClient get_socket() { return m_socket; }

        void set_uid(uint64_t id) { uId = id; };
        uint64_t get_uid() { return uId; }

        void close_client();

        int wait_server_response();
        
        void check_server_liveness();
};