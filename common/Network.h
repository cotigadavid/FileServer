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

    static int init_server_tls(const std::string& cert_path, const std::string& key_path);
    static int init_client_tls(bool verify_peer = false);
    static int wrap_server_connection(int fd);
    static int wrap_client_connection(int fd);
    static void cleanup_tls();
    static void close_tls(int fd);
    static void close_connection(int fd);  // Close both TLS and socket

    static int send_raw(int fd, const void* data, size_t len);      // fixed-size send (TLS aware)
    static ssize_t read_some(int fd, void* buf, size_t len);        // read up to len (TLS aware)
};