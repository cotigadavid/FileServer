#include "Server.h"
#include "ThreadPool.h"
#include "Network.h"
#include "Database.h"
#include "AuthManager.h"

#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <filesystem>

void initialize_schema(Database& db);

Server::Server(int port, int num_threads)
    : server_fd(-1), port(port), running(false),
      thread_pool(nullptr), db(nullptr), auth_manager(nullptr)
{
    thread_pool = new ThreadPool(num_threads);
}

Server::~Server() {
    stop();
    delete auth_manager;
    delete db;
    delete thread_pool;
}

bool Server::initialize() {
    try {
        db = new Database("server.db");
        initialize_schema(*db);
        
        std::cout << "Database initialized successfully.\n";
        
        auth_manager = new AuthManager(db->get_handle());
        
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return false;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return false;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    memset(&(server_addr.sin_zero), '\0', 8);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(server_fd);
        perror("bind failed");
        return false;
    }

    if (listen(server_fd, 5) == -1) {
        close(server_fd);
        perror("listen failed");
        return false;
    }

    running = true;
    return true;
}

void Server::run() {
    if (!running) {
        std::cerr << "Server not initialized. Call initialize() first.\n";
        return;
    }

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (running) {
        std::cout << "Server listening on port " << port << "...\n";

        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd == -1) {
            perror("accept failed");
            continue;
        }
        
        std::cout << "Client connected\n";

        // Capture necessary state for the worker thread
        AuthManager* auth_ptr = auth_manager;
        
        thread_pool->submit([client_fd, auth_ptr, this]() {
            this->handleClient(client_fd);
        });
    }
}

void Server::stop() {
    running = false;
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
}

void Server::handleClient(int client_fd) {
    char command[5] = {0};
    if (Network::recv_all(client_fd, command, 5) <= 0) {
        perror("failed to receive command");
        close(client_fd);
        return;
    }

    std::cout << "Command: " << command << "\n";

    try {
        std::cout << "About to handle command\n";
        handleCommand(client_fd, command);
        std::cout << "Command handled successfully\n";
    } catch (const std::exception& ex) {
        std::cerr << "Exception in handleCommand: " << ex.what() << "\n";
    } catch (...) {
        std::cerr << "Unknown exception in handleCommand\n";
    }

    std::cout << "Closing connection\n";
    close(client_fd);
}

void Server::handleCommand(int client_fd, const char* command) {
    std::cout << "handleCommand called with: " << command << "\n";
    
    if (strcmp(command, "send") == 0) {
        handleGetFile(client_fd);
    }
    else if (strcmp(command, "get.") == 0) {
        handleSendFile(client_fd);
    }
    else if (strcmp(command, "crte") == 0) {
        handleCreateUser(client_fd);
    }
    else if (strcmp(command, "lgin") == 0) {
        handleLogin(client_fd);
    }
    else if (strcmp(command, "lgou") == 0) {
        handleLogout(client_fd);
    }
    else {
        std::cerr << "Unknown command: " << command << "\n";
    }
    
    std::cout << "handleCommand completed\n";
}

int Server::handleGetFile(int client_fd) {
    uint32_t filename_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&filename_len_net, sizeof(filename_len_net)) <= 0) {
        perror("failed to read filename len");
        return -1;
    }

    uint32_t filename_len = ntohl(filename_len_net);

    if (filename_len >= 256) {
        std::cerr << "Filename too long\n";
        return -1;
    }

    char filename_buffer[256] = {0};
    if (Network::recv_all(client_fd, filename_buffer, filename_len) <= 0) {
        perror("failed to receive filename");
        return -1;
    }

    std::string filename(filename_buffer, filename_len);
    
    uint64_t filesize_net = 0;
    if (Network::recv_all(client_fd, (char*)&filesize_net, sizeof(filesize_net)) <= 0) {
        perror("Failed to receive file size");
        return -1;
    }

    uint64_t filesize = be64toh(filesize_net); 

    std::cout << "Receiving file: " << filename << " (" << filesize << " bytes)\n";

    std::string save_path = "server/" + filename;
    std::ofstream outfile(save_path, std::ios::binary);

    if (!outfile.is_open()) {
        perror("Could not open output file for writing");
        return -1;
    }

    char buffer[4096] = {0};
    ssize_t bytes_received;
    uint64_t total_received = 0;

    while (total_received < filesize && (bytes_received = recv(client_fd, buffer, sizeof(buffer), 0)) > 0) {
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

int Server::handleSendFile(int client_fd) {
    uint32_t filename_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&filename_len_net, sizeof(filename_len_net)) <= 0) {
        perror("failed to read filename len");
        return -1;
    }

    uint32_t filename_len = ntohl(filename_len_net);

    if (filename_len >= 256) {
        std::cerr << "Filename too long\n";
        return -1;
    }

    char filename_buffer[256] = {0};
    if (Network::recv_all(client_fd, filename_buffer, filename_len) <= 0) {
        perror("failed to receive filename");
        return -1;
    }

    std::string filename(filename_buffer, filename_len);
    std::string filepath = "server/" + filename;

    if (!std::filesystem::exists(filepath)) {
        perror("file not found");
        return -1;
    }

    auto filesize = std::filesystem::file_size(filepath);

    uint64_t filesize_net = htobe64(filesize);
    if (send(client_fd, &filesize_net, sizeof(filesize_net), 0) == -1) {
        perror("Failed to send file size");
        return -1;
    }

    std::ifstream infile(filepath, std::ios::binary);
    char buffer[4096] = {0};

    if (!infile.is_open()) {
        perror("Could not open file for reading");
        return -1;
    }

    while (true) {
        infile.read(buffer, sizeof(buffer));
        ssize_t bytes_read = infile.gcount();
        if (bytes_read <= 0)
            break;

        if (send(client_fd, buffer, bytes_read, 0) == -1) {
            perror("send failed");
            infile.close();
            return -1;
        }
    }

    infile.close();
    return 0;
}

int Server::handleCreateUser(int client_fd) {
    uint32_t username_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&username_len_net, sizeof(username_len_net)) <= 0) {
        perror("failed to read username len");
        return -1;
    }

    uint32_t username_len = ntohl(username_len_net);

    char username_buffer[256] = {0};
    if (Network::recv_all(client_fd, username_buffer, username_len) <= 0) {
        perror("failed to receive username");
        return -1;
    }

    uint32_t password_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&password_len_net, sizeof(password_len_net)) <= 0) {
        perror("failed to read password len");
        return -1;
    }

    uint32_t password_len = ntohl(password_len_net);

    char password_buffer[256] = {0};
    if (Network::recv_all(client_fd, password_buffer, password_len) <= 0) {
        perror("failed to receive password");
        return -1;
    }

    std::string username(username_buffer, username_len);
    std::string password(password_buffer, password_len);
    bool ok = auth_manager->register_user(username, password);

    std::string message = ok ? "User Created" : "Create user failed";
    if (Network::send_string(client_fd, message, "create_user_feedback") != 0) {
        perror("send feedback failed");
    }

    return ok ? 0 : -1;
}

int Server::handleLogin(int client_fd) {
    uint32_t username_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&username_len_net, sizeof(username_len_net)) <= 0) {
        perror("failed to read username len");
        return -1;
    }

    uint32_t username_len = ntohl(username_len_net);

    char username_buffer[256] = {0};
    if (Network::recv_all(client_fd, username_buffer, username_len) <= 0) {
        perror("failed to receive username");
        return -1;
    }

    uint32_t password_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&password_len_net, sizeof(password_len_net)) <= 0) {
        perror("failed to read password len");
        return -1;
    }

    uint32_t password_len = ntohl(password_len_net);

    char password_buffer[256] = {0};
    if (Network::recv_all(client_fd, password_buffer, password_len) <= 0) {
        perror("failed to receive password");
        return -1;
    }

    std::string username(username_buffer, username_len);
    std::string password(password_buffer, password_len);

    std::optional<std::string> token = auth_manager->login(username, password);
    if (token == std::nullopt) {
        std::string err("Login failed");
        Network::send_string(client_fd, err, "login_feedback");
        return -1;
    }

    // Send feedback
    std::string ok_msg("Login successful");
    if (Network::send_string(client_fd, ok_msg, "login_feedback") != 0) {
        perror("send login feedback failed");
        return -1;
    }

    // Send token
    if (Network::send_string(client_fd, token.value(), "token") != 0) {
        perror("send token failed");
        return -1;
    }

    return 0;
}

int Server::handleLogout(int client_fd) {
    
    if (!auth_manager) {
        std::cerr << "ERROR: auth_manager is nullptr!" << std::endl;
        return -1;
    }
        
    std::string token;
    
    if (Network::recv_string(client_fd, token, "token") != 0) {
        perror("failed to receive token");
        return -1;
    }

    auth_manager->logout(token);
        
    std::string message("Logged out successfully");
    Network::send_string(client_fd, message, "logout_feedback");
        
    return 0;
}
