#include <inc/Message.hpp>
#include <time.h>
#include <string>
#include <chrono>
#include <cstring>

Message::Message(){};

Message::Message(Type _type, char _payload[])
{
    type = _type;
    timestamp = std::time(nullptr);
    strcpy(payload, _payload);
};

Message::Message(Type _type, std::string _payload)
{
    type = _type;
    timestamp = std::time(nullptr);
    strcpy(payload, _payload.c_str());
};

Message::Message(Type _type, time_t _timestamp, char _author[256], char _payload[256])
{
    type = _type;
    timestamp = _timestamp;
    strcpy(author, _author);
    strcpy(payload, _payload);
}
