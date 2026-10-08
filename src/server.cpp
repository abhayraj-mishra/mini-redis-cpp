#include "server.hpp"
#include <iostream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

namespace { constexpr size_t kReadChunk = 4096; }

Server::Server(int port, KVStore& store, size_t numThreads)
    : port_(port), store_(store) {
    if (numThreads == 0) {
        // Blocking I/O → pool size must cover peak concurrent clients.
        // Start at 64; scale up with cores.
        numThreads = std::max<size_t>(64, std::thread::hardware_concurrency() * 4);
    }
    running_.store(true);
    for (size_t i = 0; i < numThreads; ++i)
        workers_.emplace_back(&Server::workerLoop, this);
}

Server::~Server() { stop(); }

void Server::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) return;
    queueCv_.notify_all();
    for (auto& t : workers_) if (t.joinable()) t.join();
    if (serverSocket_ >= 0) { close(serverSocket_); serverSocket_ = -1; }
}

std::string Server::processCommand(const std::string& line) {
    std::istringstream iss(line);
    std::string cmd, key, value;
    iss >> cmd;
    for (auto& c : cmd) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (cmd == "SET") {
        iss >> key;
        std::getline(iss, value);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);
        store_.set(key, value);
        return "OK\n";
    } else if (cmd == "GET") {
        iss >> key;
        auto v = store_.get(key);
        return v ? (*v + "\n") : "(nil)\n";
    } else if (cmd == "DEL") {
        iss >> key;
        return store_.del(key) ? "1\n" : "0\n";
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

bool Server::sendAll(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = write(fd, data.data() + sent, data.size() - sent);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

void Server::handleClient(int clientSocket) {
    std::string buffer;
    buffer.reserve(kReadChunk);
    char chunk[kReadChunk];

    while (running_.load()) {
        ssize_t n = read(clientSocket, chunk, sizeof(chunk));
        if (n <= 0) break;
        buffer.append(chunk, static_cast<size_t>(n));

        size_t pos;
        while ((pos = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, pos);
            buffer.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            std::string response = processCommand(line);
            if (!sendAll(clientSocket, response)) {
                close(clientSocket);
                return;
            }
        }
    }
    close(clientSocket);
}

void Server::workerLoop() {
    while (running_.load()) {
        int fd = -1;
        {
            std::unique_lock<std::mutex> lock(queueMtx_);
            queueCv_.wait(lock, [this] {
                return !clientQueue_.empty() || !running_.load();
            });
            if (!running_.load() && clientQueue_.empty()) return;
            fd = clientQueue_.front();
            clientQueue_.pop();
        }
        handleClient(fd);
    }
}

void Server::run() {
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) { std::cerr << "socket() failed\n"; return; }

    int opt = 1;
    setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverSocket_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind failed on port " << port_ << "\n"; return;
    }
    if (listen(serverSocket_, 512) < 0) {
        std::cerr << "listen failed\n"; return;
    }
    std::cout << "mini-redis listening on port " << port_
              << " with " << workers_.size() << " workers" << std::endl;

    while (running_.load()) {
        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        int fd = accept(serverSocket_, (sockaddr*)&clientAddr, &len);
        if (fd < 0) {
            if (!running_.load()) break;
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(queueMtx_);
            clientQueue_.push(fd);
        }
        queueCv_.notify_one();
    }
}
