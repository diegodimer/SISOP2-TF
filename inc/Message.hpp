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
    SHUTDOWN_REQ
};

class Message {
    private:
        Type type;
        uint16_t seqn; //Número de sequência
        uint16_t length; //Comprimento do payload
        time_t timestamp; // Timestamp do dado
        char author[256];
        char payload [256]; //Dados da mensagem
    public:
        Message();
        Message(Type _type, uint16_t _seqn, uint16_t _length, char _payload[]);
        Message(Type _type, uint16_t _seqn, uint16_t _length, std::string _payload);

        int send_message(int _socket);
        int receive_message(int socket);

        void set_type(Type _type) { type = _type; }
        Type get_type() { return type; };

        void set_seqn(uint16_t _seqn) { seqn =  _seqn; }
        uint16_t get_seqn() { return seqn; }

        void set_length(uint16_t _length) { length = _length; }
        uint16_t get_length() { return length; }

        time_t get_timestamp() { return timestamp; };
        void set_timestamp(time_t _timestamp) { timestamp = _timestamp; };

        void set_author(char _author[]) { strcpy(author, _author); }
        char *get_author() { return author; };

        void set_payload(char _payload[]) { strcpy(payload, _payload); }
        char *get_payload() { return payload; }

        char* get_timestamp_string() { return std::asctime(std::localtime(&timestamp)); };
};