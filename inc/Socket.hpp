#include <string>
#include <mutex>
#include <cstring>

class SocketClient {
    private:
        int m_socket;
        int m_port;
        char m_hostname[280];
    public:
        SocketClient(char _hostname[], int _port);
        SocketClient(int _socket);
        int send_message(Message _msg);
        Message* receive_message();
        int connect_to_server();
        void close_connection();

        int get_socket() { return m_socket; }
        void set_socket(int _socket) { m_socket = _socket; }

        int get_port() { return m_port; }
        void set_port(int _port) { m_port = _port; }

        char* get_hostname() { return m_hostname; }
        void set_hostname(char* _hostname) { strcpy(m_hostname, _hostname); }

        void receive_message_loop(); // may need to remove
        void send_message_loop();
        void control_girls();


};