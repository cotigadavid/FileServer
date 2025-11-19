#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <filesystem>
#include <sstream>

#include "network.h"
#include "authmanager.h"

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

int send_create_credentials(const int sockfd, const std::string& username, const std::string& password) {
    char command_buffer[] = "crte";
    if (send(sockfd, command_buffer, sizeof(command_buffer), 0) == -1) {
        perror("send command type");
        close(sockfd);
        return -1;
    }

    uint32_t username_len = htonl(username.size());
    if (send(sockfd, &username_len, sizeof(username_len), 0) == -1) {
        perror("send username len failed");
        close(sockfd);
        return -1;
    }

    if (send(sockfd, username.c_str(), username.size(), 0) == -1) {
        perror("send username failed");
        close(sockfd);
        return -1;
    }

    uint32_t password_len = htonl(password.size());
    if (send(sockfd, &password_len, sizeof(password_len), 0) == -1) {
        perror("send password len failed");
        close(sockfd);
        return -1;
    }

    if (send(sockfd, password.c_str(), password.size(), 0) == -1) {
        perror("send password failed");
        close(sockfd);
        return -1;
    }

    uint32_t feedback_len_net = 0;
    if (Network::recv_all(sockfd, (char*)&feedback_len_net, sizeof(feedback_len_net)) <= 0) {
        perror("failed to read feedback len");
        close(sockfd);
        return -1;
    }

    uint32_t feedback_len = ntohl(feedback_len_net);
    if (feedback_len > 10 * 1024) { // limit feedback to 10KB
        std::cerr << "feedback too large: " << feedback_len << "\n";
        close(sockfd);
        return -1;
    }

    std::string feedback;
    feedback.resize(feedback_len);
    if (Network::recv_all(sockfd, feedback.data(), feedback_len) <= 0) {
        perror("failed to receive feedback");
        close(sockfd);
        return -1;
    }

    std::cout << feedback << "\n";
    return 0;
}

int send_login_credentials(const int sockfd, const std::string& username, const std::string& password) {
    char command_buffer[] = "lgin";
    if (send(sockfd, command_buffer, sizeof(command_buffer), 0) == -1) {
        perror("send command type");
        close(sockfd);
        return -1;
    }

    uint32_t username_len = htonl(username.size());
    if (send(sockfd, &username_len, sizeof(username_len), 0) == -1) {
        perror("send username len failed");
        close(sockfd);
        return -1;
    }

    if (send(sockfd, username.c_str(), username.size(), 0) == -1) {
        perror("send username failed");
        close(sockfd);
        return -1;
    }

    uint32_t password_len = htonl(password.size());
    if (send(sockfd, &password_len, sizeof(password_len), 0) == -1) {
        perror("send password len failed");
        close(sockfd);
        return -1;
    }

    if (send(sockfd, password.c_str(), password.size(), 0) == -1) {
        perror("send password failed");
        close(sockfd);
        return -1;
    }

    uint32_t feedback_len_net = 0;
    if (Network::recv_all(sockfd, (char*)&feedback_len_net, sizeof(feedback_len_net)) <= 0) {
        perror("failed to read feedback len");
        close(sockfd);
        return -1;
    }

    uint32_t feedback_len = ntohl(feedback_len_net);
    if (feedback_len > 10 * 1024) { // limit feedback to 10KB
        std::cerr << "feedback too large: " << feedback_len << "\n";
        close(sockfd);
        return -1;
    }

    std::string feedback;
    feedback.resize(feedback_len);
    if (Network::recv_all(sockfd, feedback.data(), feedback_len) <= 0) {
        perror("failed to receive feedback");
        close(sockfd);
        return -1;
    }

    std::cout << feedback << "\n";

    uint32_t token_len_net = 0;
    if (Network::recv_all(sockfd, (char*)&token_len_net, sizeof(token_len_net)) <= 0) {
        perror("failed to read token len");
        close(sockfd);
        return -1;
    }

    uint32_t token_len = ntohl(token_len_net);
    if (token_len == 0 || token_len > 4096) { // sanity check token size
        std::cerr << "invalid token length: " << token_len << "\n";
        close(sockfd);
        return -1;
    }

    std::string token;
    token.resize(token_len);
    if (Network::recv_all(sockfd, token.data(), token_len) <= 0) {
        perror("failed to receive token");
        close(sockfd);
        return -1;
    }

    std::cout << "Token: " << token << "\n";
    return 0;
}

int send_logout(const int sockfd) {
    char command_buffer[] = "lgou";
    if (send(sockfd, command_buffer, sizeof(command_buffer), 0) == -1) {
        perror("send command type");
        close(sockfd);
        return -1;
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
        else if (command == "create_user") {
            std::string username, password;
            std::cout << "username: ";
            std::getline(std::cin, username);
            std::cout << "password: ";
            std::getline(std::cin, password);
            
            send_create_credentials(sockfd, username, password);
        }

        else if (command == "login") {
            std::string username, password;
            std::cout << "username: ";
            std::getline(std::cin, username);
            std::cout << "password: ";
            std::getline(std::cin, password);
            
            send_login_credentials(sockfd, username, password);
        }

        else if (command == "logout") {
            send_logout(sockfd);
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
