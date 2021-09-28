#include <inc/Message.hpp>
#include <time.h>
#include <string>
#include <chrono>
#include <cstring>

Message::Message(){};

Message::Message(Type _type, uint16_t _seqn, uint16_t _length, char _payload[])
{
    type = _type;
    seqn = _seqn;
    length = _length;
    timestamp = std::time(nullptr);
    strcpy(payload, _payload);
};

Message::Message(Type _type, uint16_t _seqn, uint16_t _length, std::string _payload)
{
    type = _type;
    seqn = _seqn;
    length = _length;
    timestamp = std::time(nullptr);
    strcpy(payload, _payload.c_str());
};

Message::Message(Type _type, uint16_t _seqn, uint16_t _length, time_t _timestamp, char _author[256], char _payload[256])
{
    type = _type;
    seqn = _seqn;
    length = _length;
    timestamp = _timestamp;
    strcpy(author, _author);
    strcpy(payload, _payload);
}
