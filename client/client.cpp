#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <filesystem>
#include <sstream>

#include "network.h"

// ssize_t recv_all(int sockfd, char *buf, size_t len) {
//     size_t total = 0;
//     ssize_t bytes_left = len;
//     ssize_t n;

//     while (total < len) {
//         n = recv(sockfd, buf + total, bytes_left, 0);
//         if (n <= 0) {
//             return n;
//         }
//         total += n;
//         bytes_left -= n;
//     }
//     return total;
// }

int send_file(const int sockfd, const std::string& filepath) {

    if (!std::filesystem::exists(filepath)) {
        perror("open file failed");
        return 1;
    }

    std::string filename = std::filesystem::path(filepath).filename().string();
    auto filesize = std::filesystem::file_size(filepath);

    if (filename.size() >= 256) {
        perror("filename too long");
        return 1;
    }

    char command_buffer[] = "send";
    if (send(sockfd, command_buffer, sizeof(command_buffer), 0) == -1) {
        perror("send command type");
        close(sockfd);
        return -1;
    }

    uint32_t filename_len = htonl(filename.size());
    if (send(sockfd, &filename_len, sizeof(filename_len), 0) == -1) {
        perror("send file name len failed");
        close(sockfd);
        return -1;
    }

    if (send(sockfd, filename.c_str(), filename.size(), 0) == -1) {
        perror("send file name failed");
        close(sockfd);
        return -1;
    }

    uint64_t filesize_net = htobe64(filesize);
    if (send(sockfd, &filesize_net, sizeof(filesize_net), 0) == -1) {
        perror("Failed to send file size");
        close(sockfd);
        return -1;
    }

    std::ifstream infile(filepath, std::ios::binary);
    char buffer[4096] = {0};

    if (!infile.is_open()) {
        perror("Could not open file.txt for reading");
        return 1;
    }

    while (true) {
        infile.read(buffer, sizeof(buffer));
        ssize_t bytes_read = infile.gcount();
        if (bytes_read <= 0)
            break;

        if (send(sockfd, buffer, bytes_read, 0) == -1) {
            perror("send failed");
            close(sockfd);
            infile.close();
            return -1;
        }
    }

    infile.close();

    return 0;
}

int get_file(const int sockfd, const std::string& filepath) {
    
    char command_buffer[] = "get.";
    if (send(sockfd, command_buffer, sizeof(command_buffer), 0) == -1) {
        perror("send command type");
        close(sockfd);
        return -1;
    }
    
    std::string filename = std::filesystem::path(filepath).filename().string();

    uint32_t filename_len = htonl(filename.size());
    if (send(sockfd, &filename_len, sizeof(filename_len), 0) == -1) {
        perror("send file name len failed");
        close(sockfd);
        return -1;
    }

    if (send(sockfd, filename.c_str(), filename.size(), 0) == -1) {
        perror("send file name failed");
        close(sockfd);
        return -1;
    }

    if (filename.size() >= 256) {
        std::cerr << "Filename too long\n";
        close(sockfd);
        return -1;
    }

    uint64_t filesize_net = 0;
    if (Network::recv_all(sockfd, (char*)&filesize_net, sizeof(filesize_net)) <= 0) {
        perror("Failed to receive file size");
        close(sockfd);
        return -1;
    }

    uint64_t filesize = be64toh(filesize_net); 

    std::cout << "Receiving file: " << filename << " (" << filesize << " bytes)\n";

    std::string save_path = "client/" + filename;
    std::ofstream outfile(save_path, std::ios::binary);

    if (!outfile.is_open()) {
        perror("Could not open output file for writing");
        return 1;
    }

    char buffer[4096] = {0};
    ssize_t bytes_received;
    uint64_t total_received = 0;

    while (total_received < filesize && (bytes_received = recv(sockfd, buffer, sizeof(buffer), 0)) > 0) {
        size_t to_write = std::min((size_t)bytes_received, (size_t)(filesize - total_received));
        outfile.write(buffer, to_write);
        total_received += to_write;
    }
    
    outfile.close();

    if (bytes_received < 0) {
        perror("recv failed");
    }
    else if (total_received == filesize) {
        std::cout << "File transfer complete. Received " << total_received << " bytes.\n";
    }
    else {
        std::cerr << "File transfer incomplete. Expected: " << filesize << ", Received: " << total_received << "\n";
    }
    return 0;
}

int main() {

    std::string line;
    
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line); 
        std::string command;
        std::string filename;

        ss >> command;

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        if (sockfd == -1) {
            perror("socket failed");
            return 1;
        }

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8080);

        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            perror("connect failed");
            close(sockfd);
            return 1;
        }

        if (command == "send") {
            ss >> filename;
            if (filename.empty())
                std::cerr << "Usage: send <filename>\n";
            else {
                std::cout << "Got here\n";
                send_file(sockfd, filename);
            }
        } 
        else if (command == "get") {
            ss >> filename;
            if (filename.empty()) 
                std::cerr << "Usage: get <filename>\n";
            else 
                get_file(sockfd, filename);  
        } 
        else if (command == "quit") {
            std::cout << "Disconnecting from server.\n";
            // Optionally, send a "QUIT" message to the server so it knows
            break; // Exit the loop
        } 
        else {
            std::cerr << "Unknown command: '" << command << "'\n";
        }
        close(sockfd);
    }
    return 0;
}
