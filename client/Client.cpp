#include "Client.h"
#include "../common/Network.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <filesystem>

Client::Client(const std::string& ip, int port)
    : server_ip(ip), server_port(port), sockfd(-1), connected(false), logged_in(false)
{
}

Client::~Client() {
    closeConnection();
    // (Keep g_client_ctx for process lifetime; free at program end if desired)
}

bool Client::connectToServer() {
    if (connected) 
        return true;
    if (Network::init_client_tls() != 0) 
        return false; // TLS ctx

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket failed");
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect failed");
        close(sockfd);
        sockfd = -1;
        return false;
    }

    if (Network::wrap_client_connection(sockfd) != 0) { 
        std::cerr << "TLS handshake failed\n"; 
        close(sockfd); 
        sockfd = -1; 
        return false; 
    }
    connected = true;
    return true;
}

void Client::closeConnection() {
    Network::close_tls(sockfd);
    // ensure TLS cleanup if implemented (optional call)
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
    }
    connected = false;
}

bool Client::createUser(const std::string& username, const std::string& password) {
    if (!connectToServer()) return false;

    char command_buffer[] = "crte";
    if (Network::send_raw(sockfd, command_buffer, 5) != 0) { perror("send command type"); closeConnection(); return false; }

    if (Network::send_string(sockfd, username, "username") != 0) {
        closeConnection();
        return false;
    }
    if (Network::send_string(sockfd, password, "password") != 0) {
        closeConnection();
        return false;
    }

    // Receive feedback
    std::string feedback;
    if (Network::recv_string(sockfd, feedback, "create_user_feedback") != 0) {
        closeConnection();
        return false;
    }

    std::cout << feedback << "\n";
    closeConnection();
    return true;
}

bool Client::login(const std::string& username, const std::string& password) {
    if (!connectToServer()) return false;

    char command_buffer[] = "lgin";
    if (Network::send_raw(sockfd, command_buffer, 5) != 0) { perror("send command type"); closeConnection(); return false; }

    if (Network::send_string(sockfd, username, "username") != 0) {
        closeConnection();
        return false;
    }
    if (Network::send_string(sockfd, password, "password") != 0) {
        closeConnection();
        return false;
    }

    // Receive feedback
    std::string feedback;
    if (Network::recv_string(sockfd, feedback, "login_feedback") != 0) {
        closeConnection();
        return false;
    }
    std::cout << feedback << "\n";

    // Receive token
    std::string received_token;
    if (Network::recv_string(sockfd, received_token, "token") != 0) {
        closeConnection();
        return false;
    }

    if (feedback == "Login successful") {
        token = received_token;
        logged_in = true;
        std::cout << "Token: " << token << "\n";
    }
    else {
        std::cout << "Failed to connect: Details ...";
    }
    
    closeConnection();
    return true;
}

bool Client::logout() {
    if (!logged_in) {
        std::cerr << "Not logged in\n";
        return false;
    }

    if (!connectToServer()) return false;

    char command_buffer[] = "lgou";
    if (Network::send_raw(sockfd, command_buffer, 5) != 0) { perror("send command type"); closeConnection(); return false; }

    if (Network::send_string(sockfd, token, "token") != 0) {
        perror("send token failed");
        closeConnection();
        return false;
    }

    // Receive feedback from server
    std::string feedback;
    if (Network::recv_string(sockfd, feedback, "logout_feedback") != 0) {
        std::cerr << "Failed to receive logout feedback\n";
        closeConnection();
        return false;
    }
    
    std::cout << feedback << "\n";

    token = "";
    logged_in = false;
    closeConnection();
    return true;
}

bool Client::uploadFile(const std::string& filepath) {
    if (!connectToServer()) return false;

    if (!std::filesystem::exists(filepath)) {
        std::cerr << "File does not exist: " << filepath << "\n";
        return false;
    }

    char command_buffer[] = "send";
    if (Network::send_raw(sockfd, command_buffer, 5) != 0) { perror("send command type"); closeConnection(); return false; }

    if (Network::send_string(sockfd, token, "token") != 0) {
        perror("send token failed");
        closeConnection();
        return false;
    }

    std::string filename = std::filesystem::path(filepath).filename().string();
    auto filesize = std::filesystem::file_size(filepath);

    if (filename.size() >= 256) {
        std::cerr << "Filename too long\n";
        return false;
    }

    if (Network::send_string(sockfd, filename, "filename") != 0) {
        closeConnection();
        return false;
    }

    uint64_t filesize_net = htobe64(filesize);
    if (Network::send_raw(sockfd, &filesize_net, sizeof(filesize_net)) != 0) { perror("Failed to send file size"); closeConnection(); return false; }

    std::ifstream infile(filepath, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << "Could not open file for reading\n";
        closeConnection();
        return false;
    }

    char buffer[4096];
    while (true) {
        infile.read(buffer, sizeof(buffer));
        ssize_t bytes_read = infile.gcount();
        if (bytes_read <= 0) break;

        if (Network::send_raw(sockfd, buffer, (size_t)bytes_read) != 0) { perror("send failed"); infile.close(); closeConnection(); return false; }
    }

    infile.close();
    closeConnection();
    std::cout << "File uploaded successfully\n";
    return true;
}

bool Client::downloadFile(const std::string& filename) {
    if (!connectToServer()) return false;

    char command_buffer[] = "get.";
    if (Network::send_raw(sockfd, command_buffer, 5) != 0) { perror("send command type"); closeConnection(); return false; }

    if (Network::send_string(sockfd, token, "token") != 0) {
        perror("send token failed");
        closeConnection();
        return false;
    }

    if (Network::send_string(sockfd, filename, "filename") != 0) {
        closeConnection();
        return false;
    }

    uint64_t filesize_net = 0;
    if (Network::recv_all(sockfd, (char*)&filesize_net, sizeof(filesize_net)) <= 0) {
        perror("Failed to receive file size");
        closeConnection();
        return false;
    }

    uint64_t filesize = be64toh(filesize_net);
    std::cout << "Receiving file: " << filename << " (" << filesize << " bytes)\n";

    std::string save_path = "client/" + filename;
    std::ofstream outfile(save_path, std::ios::binary);

    if (!outfile.is_open()) {
        std::cerr << "Could not open output file for writing\n";
        closeConnection();
        return false;
    }

    char buffer[4096];
    ssize_t bytes_received;
    uint64_t total_received = 0;

    while (total_received < filesize) {
        ssize_t bytes_received = Network::read_some(sockfd, buffer, sizeof(buffer));
        if (bytes_received <= 0) break;
        size_t to_write = std::min((size_t)bytes_received, (size_t)(filesize - total_received));
        outfile.write(buffer, to_write);
        total_received += to_write;
    }

    outfile.close();

    if (total_received == filesize) {
        std::cout << "File transfer complete. Received " << total_received << " bytes.\n";
        closeConnection();
        return true;
    } else {
        std::cerr << "File transfer incomplete. Expected: " << filesize << ", Received: " << total_received << "\n";
        closeConnection();
        return false;
    }
}

bool Client::list() {
    if (!logged_in) {
        std::cerr << "Not logged in\n";
        return false;
    }
    if (!connectToServer()) return false;

    char command_buffer[] = "list";
    if (Network::send_raw(sockfd, command_buffer, 5) != 0) { perror("send command type"); closeConnection(); return false; }

    if (Network::send_string(sockfd, token, "token") != 0) {
        perror("send token failed");
        closeConnection();
        return false;
    }

    uint32_t count_net = 0;
    if (Network::recv_all(sockfd, (char*)&count_net, sizeof(count_net)) <= 0) {
        perror("recv file count failed");
        closeConnection();
        return false;
    }
    uint32_t count = ntohl(count_net);
    std::cout << "Files (" << count << "):\n";
    for (uint32_t i = 0; i < count; ++i) {
        std::string fname;
        if (Network::recv_string(sockfd, fname, "filename") != 0) {
            perror("recv filename failed");
            closeConnection();
            return false;
        }
        std::cout << " - " << fname << "\n";
    }

    closeConnection();
    return true;
}