class RM_info {
    public:
    int socketfd;
    int RM_id;
    RM_info(int fd, int id) {this->socketfd = fd; this->RM_id = id;}
    RM_info() {;}
};