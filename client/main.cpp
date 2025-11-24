#include "Client.h"
#include <iostream>
#include <sstream>
#include <string>

int main() {
    Client client("127.0.0.1", 8080);
    std::string line;

    std::cout << "File Server Client\n";
    std::cout << "Commands: create_user, login, logout, send <file>, get <file>, list, quit\n\n";

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, line);

        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command == "send") {
            std::string filename;
            ss >> filename;
            if (filename.empty()) {
                std::cerr << "Usage: send <filename>\n";
            } else {
                client.uploadFile(filename);
            }
        }
        else if (command == "get") {
            std::string filename;
            ss >> filename;
            if (filename.empty()) {
                std::cerr << "Usage: get <filename>\n";
            } else {
                client.downloadFile(filename);
            }
        }
        else if (command == "create_user") {
            std::string username, password;
            std::cout << "username: ";
            std::getline(std::cin, username);
            std::cout << "password: ";
            std::getline(std::cin, password);

            client.createUser(username, password);
        }
        else if (command == "login") {
            std::string username, password;
            std::cout << "username: ";
            std::getline(std::cin, username);
            std::cout << "password: ";
            std::getline(std::cin, password);

            client.login(username, password);
        }
        else if (command == "logout") {
            client.logout();
        }
        else if (command == "list") {
            std::cout << "ENTER LSIT\n";
            client.list();
        }
        else if (command == "quit") {
            std::cout << "Exiting...\n";
            break;
        }
        else {
            std::cerr << "Unknown command: '" << command << "'\n";
        }
    }

    return 0;
}
