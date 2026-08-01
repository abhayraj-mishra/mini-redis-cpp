#include "server.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

Server::Server(int port, KVStore& store) : port_(port), store_(store) {}

std::string Server::processCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd, key, value;
    iss >> cmd;

    for (auto& c : cmd) c = toupper(c);

    if (cmd == "SET") {
        iss >> key;
        std::getline(iss, value);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);
        store_.set(key, value);
        store_.appendToLog("SET " + key + " " + value);
        return "OK\n";
    } else if (cmd == "GET") {
        iss >> key;
        auto v = store_.get(key);
        return v ? (*v + "\n") : "(nil)\n";
    } else if (cmd == "DEL") {
        iss >> key;
        bool removed = store_.del(key);
        store_.appendToLog("DEL " + key);
        return removed ? "1\n" : "0\n";
    } else if (cmd == "EXISTS") {
        iss >> key;
        return store_.exists(key) ? "1\n" : "0\n";
    } else if (cmd == "EXPIRE") {
        int seconds = 0;
        iss >> key >> seconds;
        store_.expire(key, seconds);
        return "OK\n";
    } else if (cmd == "PING") {
        return "PONG\n";
    }

    return "ERR unknown command\n";
}

void Server::handleClient(int clientSocket) {
    char buffer[4096];
    while (true) {
        ssize_t bytesRead = read(clientSocket, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) break; // client disconnected

        buffer[bytesRead] = '\0';
        std::string line(buffer);

        // strip trailing newline/carriage return
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();

        if (line.empty()) continue;

        std::string response = processCommand(line);
        write(clientSocket, response.c_str(), response.size());
    }
    close(clientSocket);
}

void Server::run() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Failed to create socket\n";
        return;
    }

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed on port " << port_ << "\n";
        return;
    }

    if (listen(serverSocket, 128) < 0) {
        std::cerr << "Listen failed\n";
        return;
    }

    std::cout << "mini-redis listening on port " << port_ << "...\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) continue;

        // One thread per client for now -- fine for a resume project demo,
        // replace with a thread pool (see TODO in server.hpp) if you want
        // to show off handling higher concurrency.
        std::thread(&Server::handleClient, this, clientSocket).detach();
    }

    close(serverSocket);
}
