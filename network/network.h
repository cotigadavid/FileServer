#pragma once

#include <stddef.h>
#include <unistd.h>

class Network {
public:
    static int send_file(int client_fd);
    static int get_file(int client_fd);

    static ssize_t recv_all(int sockfd, char *buf, size_t len);
};