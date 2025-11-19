#include "network.h"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <climits>
#include <cstdint>

ssize_t Network::recv_all(int sockfd, char *buf, size_t len) {
    size_t total = 0;
    ssize_t n;
    while (total < len) {
        n = recv(sockfd, buf + total, len - total, 0);
        if (n <= 0) {
            return n;
        }
        total += n;
    }
    return (ssize_t)total;
}

int Network::send_bytes(int client_fd, const void* data, size_t size, const std::string& debug_name) {
    if (size > UINT32_MAX) {
        std::cerr << debug_name << " too large to send: " << size << "\n";
        return -1;
    }
    uint32_t len_net = htonl((uint32_t)size);
    if (send(client_fd, &len_net, sizeof(len_net), 0) == -1) {
        perror(("send " + debug_name + " length failed").c_str());
        return -1;
    }
    const char* p = static_cast<const char*>(data);
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(client_fd, p + sent, size - sent, 0);
        if (n <= 0) {
            perror(("send " + debug_name + " data failed").c_str());
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

int Network::recv_bytes(int client_fd, std::vector<char>& buffer, const std::string& debug_name) {
    uint32_t len_net = 0;
    if (recv_all(client_fd, (char*)&len_net, sizeof(len_net)) <= 0) {
        perror(("recv " + debug_name + " length failed").c_str());
        return -1;
    }
    uint32_t len = ntohl(len_net);
    const uint32_t MAX = 10u * 1024u * 1024u; // 10MB limit
    if (len > MAX) {
        std::cerr << debug_name << " too large: " << len << " bytes\n";
        return -1;
    }
    buffer.resize(len);
    if (len == 0) return 0;
    if (recv_all(client_fd, buffer.data(), len) <= 0) {
        perror(("recv " + debug_name + " data failed").c_str());
        return -1;
    }
    return 0;
}

int Network::send_string(int client_fd, const std::string& str, const std::string& debug_name) {
    return send_bytes(client_fd, str.data(), str.size(), debug_name);
}

int Network::recv_string(int client_fd, std::string& str, const std::string& debug_name) {
    std::vector<char> buffer;
    if (recv_bytes(client_fd, buffer, debug_name) != 0) return -1;
    str.assign(buffer.begin(), buffer.end());
    return 0;
}

int Network::get_file(int client_fd) {
    // ...existing code (if any)...
    return 0;
}

int Network::send_file(int client_fd) {
    // ...existing code (if any)...
    return 0;
}