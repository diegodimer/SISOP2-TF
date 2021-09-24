#include <string>
#include <chrono>
/* Message é a classe que vai ser trocada pelo socket das aplicações,
*então um client envia um socket ao servidor e recebe um socket do servidor
* precisamos de atributos que diferenciem as mensagens
*/

// Tipo da mensagem: pode ser um tweet novo, um follower novo ou uma mensagem de controle qualquer
enum Type {
     NEW_TWEET,
     NEW_FOLLOW,
     CONTROL_MSG
};

class Message {
    private:
        Type type;
        time_t timestamp;
        char body[280];
        char author[280];
    public:
        Message();
        Message(Type _type, char _body[], char _author[]);
        Message(Type _type, std::string _body, std::string _author);
        int send_message(int _socket);
        int receive_message(int socket);
        Type get_type() { return type; };
        char* get_author() { return author; };
        char* get_body() { return body; };
        char* get_timestamp_string() { return std::asctime(std::localtime(&timestamp)); };
        time_t get_timestamp() { return timestamp; };
};