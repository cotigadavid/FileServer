#include "network.h"

#include <arpa/inet.h>

ssize_t Network::recv_all(int sockfd, char *buf, size_t len) {
    size_t total = 0;
    ssize_t bytes_left = len;
    ssize_t n;

    while (total < len) {
        n = recv(sockfd, buf + total, bytes_left, 0);
        if (n <= 0) {
            return n;
        }
        total += n;
        bytes_left -= n;
    }
    return total;
}

int Network::get_file(int client_fd) {

}

int Network::send_file(int client_fd) {
    
}