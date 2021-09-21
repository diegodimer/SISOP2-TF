#ifndef DATA_STRUCTURES_HPP_INCLUDED
#define DATA_STRUCTURES_HPP_INCLUDED

typedef struct __packet{
    uint16_t type; //Tipo do pacote (p.ex. DATA | CMD)
    uint16_t seqn; //Número de sequência
    uint16_t length; //Comprimento do payload
    uint16_t timestamp; // Timestamp do dado
    char _payload [256]; //Dados da mensagem
} packet;


#endif // DATA_STRUCTURES_HPP_INCLUDED
