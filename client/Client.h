#pragma once

#include <string>
#include <optional>

class Client {
private:
    std::string server_ip;
    int server_port;
    int sockfd;
    std::string token;
    bool connected;
    bool logged_in;

    // Helper method to establish connection
    bool connectToServer();
    void closeConnection();

public:
    Client(const std::string& ip = "127.0.0.1", int port = 8080);
    ~Client();

    bool createUser(const std::string& username, const std::string& password);
    bool login(const std::string& username, const std::string& password);
    bool logout();

    bool uploadFile(const std::string& filepath);
    bool downloadFile(const std::string& filename);
    bool list();

    bool isLoggedIn() const { return logged_in; }
    bool isConnected() const { return connected; }
    const std::string& getToken() const { return token; }
};
