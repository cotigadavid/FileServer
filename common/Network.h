#pragma once

#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <arpa/inet.h>

class Network {
public:
    static ssize_t recv_all(int sockfd, char *buf, size_t len);

    static int send_bytes(int client_fd, const void* data, size_t size, const std::string& debug_name);
    static int recv_bytes(int client_fd, std::vector<char>& buffer, const std::string& debug_name);

    static int send_string(int client_fd, const std::string& str, const std::string& debug_name);
    static int recv_string(int client_fd, std::string& str, const std::string& debug_name);

    static int get_file(int client_fd);
    static int send_file(int client_fd);
};