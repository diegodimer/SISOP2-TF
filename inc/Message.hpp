#include <string>

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
        std::string body;
        std::string author;
        time_t timestamp;
    public:
        Message();
        Message(Type _type, std::string _body, std::string _author, time_t _timestamp);
};