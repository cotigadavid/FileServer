#pragma once

#include <string>

// Forward declarations
class ThreadPool;
class Database;
class AuthManager;

class Server {
private:
    int server_fd;
    int port;
    bool running;
    
    ThreadPool* thread_pool;
    Database* db;
    AuthManager* auth_manager;

    // Private helper methods
    void handleClient(int client_fd);
    void handleCommand(int client_fd, const char* command);
    
    // Command handlers
    int handleGetFile(int client_fd);
    int handleSendFile(int client_fd);
    int handleCreateUser(int client_fd);
    int handleLogin(int client_fd);
    int handleLogout(int client_fd);

public:
    Server(int port = 8080, int num_threads = 4);
    ~Server();

    bool initialize();
    void run();
    void stop();
};
