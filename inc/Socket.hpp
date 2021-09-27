#include <string>
#include <mutex>

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
        void receive_message_loop(); // may need to remove
        void send_message_loop();
        void control_girls();
};