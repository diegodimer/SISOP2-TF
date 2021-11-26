#include <string>
#include <mutex>
#include <cstring>

extern int m_socket_num;
extern int m_port;
extern char m_hostname[280];

class SocketClient {
    private:
    public:
        SocketClient() {};
        SocketClient(char _hostname[], int _port);
        SocketClient(int _socket);
        int send_message(Message _msg);
        int send_message_no_retry(Message);

        Message* receive_message(int *error_code);
        Message* receive_message_no_retry();
        int connect_to_server();
        void close_connection();

        int get_socket() { return m_socket_num; }
        void set_socket(int _socket) { m_socket_num = _socket; }

        int get_port() { return m_port; }
        void set_port(int _port) { m_port = _port; }

        char* get_hostname() { return m_hostname; }
        void set_hostname(char* _hostname) { strcpy(m_hostname, _hostname); }
};