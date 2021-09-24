#include <inc/Message.hpp>
#include <time.h>
#include <string>
#include <chrono>
#include <cstring>

Message::Message() {
};

Message::Message(Type _type, std::string _body, std::string _author) {
    type = _type;
    strcpy(body, _body.c_str());
    strcpy(author, _author.c_str());
    timestamp = std::time(nullptr);
};

Message::Message(Type _type, char _body[], char _author[]) {
    type = _type;
    strcpy(body, _body);
    strcpy(author, _author);
    timestamp = std::time(nullptr);
};

