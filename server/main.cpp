#include "Server.h"
#include <iostream>
#include <csignal>

Server* g_server = nullptr;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down...\n";
    if (g_server) {
        g_server->stop();
    }
    exit(signum);
}

int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    Server server(8080, 4);
    g_server = &server;

    if (!server.initialize()) {
        std::cerr << "Failed to initialize server\n";
        return 1;
    }

    std::cout << "File Server starting...\n";
    server.run();

    return 0;
}
