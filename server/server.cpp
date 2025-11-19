#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <filesystem>

#include "threadpool.h"
#include "network.h"
#include "database.h"
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

void initialize_schema(Database& db);

int get_file(int client_fd) {

    uint32_t filename_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&filename_len_net, sizeof(filename_len_net)) <= 0) {
        perror("failed to read filenam len");
        close(client_fd);
        return -1;
    }

    uint32_t filename_len = ntohl(filename_len_net);

    std::cout << filename_len << "\n";

    if (filename_len >= 256) {
        std::cerr << "Filename too long\n";
        close(client_fd);
        return -1;
    }

    char filename_buffer[256] = {0};
    if (Network::recv_all(client_fd, filename_buffer, filename_len) <= 0) {
        perror("failed to receive filename");
        close(client_fd);
        return -1;
    }

    std::string filename(filename_buffer, filename_len);
    
    std::cout << filename << "\n";
    
    uint64_t filesize_net = 0;
    if (Network::recv_all(client_fd, (char*)&filesize_net, sizeof(filesize_net)) <= 0) {
        perror("Failed to receive file size");
        close(client_fd);
        return -1;
    }

    uint64_t filesize = be64toh(filesize_net); 

    std::cout << "Receiving file: " << filename << " (" << filesize << " bytes)\n";

    std::string save_path = "server/" + filename;
    std::ofstream outfile(save_path, std::ios::binary);

    if (!outfile.is_open()) {
        perror("Could not open output file for writing");
        close(client_fd);
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

int send_file(int client_fd) {
    std::cout << "SEND\n";
    uint32_t filename_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&filename_len_net, sizeof(filename_len_net)) <= 0) {
        perror("failed to read filenam len");
        close(client_fd);
        return -1;
    }

    uint32_t filename_len = ntohl(filename_len_net);

    std::cout << filename_len << " " << filename_len_net << "\n";

    if (filename_len >= 256) {
        std::cerr << "Filename too long\n";
        close(client_fd);
        return -1;
    }

    char filename_buffer[256] = {0};
    if (Network::recv_all(client_fd, filename_buffer, filename_len) <= 0) {
        perror("failed to receive filename");
        close(client_fd);
        return -1;
    }

    std::string filename(filename_buffer, filename_len);
    
    std::cout << filename << "\n";

    std::string filepath = "server/" + filename;

    if (!std::filesystem::exists(filepath)) {
        perror("open file failed");
        return 1;
    }

    auto filesize = std::filesystem::file_size(filepath);

    if (filename.size() >= 256) {
        perror("filename too long");
        return 1;
    }

    // filename_len = htonl(filename.size());
    // if (send(client_fd, &filename_len, sizeof(filename_len), 0) == -1) {
    //     perror("send file name len failed");
    //     close(client_fd);
    //     return -1;
    // }

    // if (send(client_fd, filename.c_str(), filename.size(), 0) == -1) {
    //     perror("send file name failed");
    //     close(client_fd);
    //     return -1;
    // }

    std::cout << "sending file size\n";

    uint64_t filesize_net = htobe64(filesize);
    if (send(client_fd, &filesize_net, sizeof(filesize_net), 0) == -1) {
        perror("Failed to send file size");
        close(client_fd);
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

        if (send(client_fd, buffer, bytes_read, 0) == -1) {
            perror("send failed");
            close(client_fd);
            infile.close();
            return -1;
        }
    }

    infile.close();
    return 0;
}

int create_user(AuthManager* auth_manager, const int client_fd) {
    uint32_t username_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&username_len_net, sizeof(username_len_net)) <= 0) {
        perror("failed to read username len");
        close(client_fd);
        return -1;
    }

    uint32_t username_len = ntohl(username_len_net);

    char username_buffer[256] = {0};
    if (Network::recv_all(client_fd, username_buffer, username_len) <= 0) {
        perror("failed to receive username");
        close(client_fd);
        return -1;
    }

    uint32_t password_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&password_len_net, sizeof(password_len_net)) <= 0) {
        perror("failed to read password len");
        close(client_fd);
        return -1;
    }

    uint32_t password_len = ntohl(password_len_net);

    char password_buffer[256] = {0};
    if (Network::recv_all(client_fd, password_buffer, password_len) <= 0) {
        perror("failed to receive password");
        close(client_fd);
        return -1;
    }

    std::string username(username_buffer, username_len);
    std::string password(password_buffer, password_len);
    bool ok = auth_manager->register_user(username, password);

    std::string message = ok ? "User Created" : "Create user failed";
    if (Network::send_string(client_fd, message, "create_user_feedback") != 0) {
        // if sending feedback fails, just log
        perror("send feedback failed");
    }

    return ok ? 0 : -1;
}

int login(AuthManager* auth_manager, const int client_fd) {

    std::cout << "login\n";

    uint32_t username_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&username_len_net, sizeof(username_len_net)) <= 0) {
        perror("failed to read username len");
        close(client_fd);
        return -1;
    }

    uint32_t username_len = ntohl(username_len_net);

    char username_buffer[256] = {0};
    if (Network::recv_all(client_fd, username_buffer, username_len) <= 0) {
        perror("failed to receive username");
        close(client_fd);
        return -1;
    }

    uint32_t password_len_net = 0;
    if (Network::recv_all(client_fd, (char*)&password_len_net, sizeof(password_len_net)) <= 0) {
        perror("failed to read password len");
        close(client_fd);
        return -1;
    }

    std::cout << "here\n";

    uint32_t password_len = ntohl(password_len_net);

    char password_buffer[256] = {0};
    if (Network::recv_all(client_fd, password_buffer, password_len) <= 0) {
        perror("failed to receive password");
        close(client_fd);
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

    // SEND FEEDBACK
    std::string ok_msg("Login successful");
    if (Network::send_string(client_fd, ok_msg, "login_feedback") != 0) {
        perror("send login feedback failed");
        return -1;
    }

    // SEND TOKEN (as length-prefixed string)
    if (Network::send_string(client_fd, token.value(), "token") != 0) {
        perror("send token failed");
        return -1;
    }

    return 0;
}

int logout(AuthManager* auth_manager, const int client_fd) {
    // RECEIVE TOKEN

    //SEND FEEDBACK
}

int main() {
    Database* db = nullptr;
    AuthManager* auth_manager = nullptr;

    try {
        db = new Database("server.db");   // creates the file if not exists
        initialize_schema(*db);

        std::cout << "Database initialized successfully.\n";

        auth_manager = new AuthManager(db->get_handle());

    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        delete auth_manager;
        delete db;
        return 1;
    }
    
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len = sizeof(client_addr);

    // Create thread pool with 4 worker threads
    ThreadPool pool(4);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);
    memset(&(server_addr.sin_zero), '\0', 8);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(server_fd);
        perror("bind failed");
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        close(server_fd);
        perror("listen failed");
        return 1;
    }

    while (true) {
        std::cout << "Server listening on port 8080...\n";

        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd == -1) {
            perror("accept failed");
            continue;
        }
        std::cout << "Client connected\n";

        // Submit client handling to thread pool
        pool.submit([client_fd, auth_manager]() {
            char command[5] = {0};
            if (Network::recv_all(client_fd, command, 5) <= 0) {
                perror("failed to receive command");
                close(client_fd);
                return;
            }

            std::cout << "Command: " << command << "\n";

            if (strcmp(command, "send") == 0) {
                get_file(client_fd);
            }
            else if (strcmp(command, "get.") == 0) {
                send_file(client_fd);
            }
            else if (strcmp(command, "crte") == 0) {
                create_user(auth_manager, client_fd);
            }
            else if (strcmp(command, "lgin") == 0) {
                std::cout << "hey\n\n";
                login(auth_manager, client_fd);
            }
            else if (strcmp(command, "lgou") == 0) {
                logout(auth_manager, client_fd);
            }

            std::cout << "Closing connection\n";
            close(client_fd);
        });
    }

    close(server_fd);
    
    delete auth_manager;
    delete db;

    return 0;
}



// MAKE SERVER AND CLIENT CLASSES
// FILE FOLDERS STRUCTURE
// MORE FUNCTIONS - FUNCTION TO SEND NEAPARAT AND RECEIVE
// CHECKS - WHEN CALLING INT FUNCTIONS
// FEEDBACK EVERYWHERE