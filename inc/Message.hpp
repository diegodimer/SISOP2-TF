#include <string>
#include <chrono>
#include <cstring>
/* Message é a classe que vai ser trocada pelo socket das aplicações,
*então um client envia um socket ao servidor e recebe um socket do servidor
* precisamos de atributos que diferenciem as mensagens
*/

// Tipo da mensagem: pode ser um tweet novo, um follower novo ou uma mensagem de controle qualquer
enum Type {
    ACK = 0,
    NACK,
    FOLLOW,
    UPDATE,
    SIGN_IN,
    SHUTDOWN_REQ,
    DUMMY_MESSAGE,
    RECONNECT,
    KEEP_ALIVE,
    ELECTION_REQ
};

class Message {
    private:
        Type type;
        time_t timestamp; // Timestamp do dado
        char author[256];
        char payload [256]; //Dados da mensagem
    public:
        Message();
        Message(Type _type, char _payload[]);
        Message(Type _type, std::string _payload);
        Message(Type _type, time_t _timestamp, char _author[256], char _payload [256]); 

        void set_type(Type _type) { type = _type; }
        Type get_type() { return type; };

        time_t get_timestamp() { return timestamp; };
        void set_timestamp(time_t _timestamp) { timestamp = _timestamp; };

        void set_author(char _author[]) { strcpy(author, _author); }
        char *get_author() { return author; };

        void set_payload(char _payload[]) { strcpy(payload, _payload); }
        char *get_payload() { return payload; }

        char* get_timestamp_string() { return std::asctime(std::localtime(&timestamp)); };
};