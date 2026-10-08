#pragma once
#include "store.hpp"
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class Server {
public:
    Server(int port, KVStore& store, size_t numThreads = 0);
    ~Server();
    void run();
    void stop();

private:
    void handleClient(int clientSocket);
    std::string processCommand(const std::string& line);
    void workerLoop();
    bool sendAll(int fd, const std::string& data);

    int port_;
    KVStore& store_;

    int serverSocket_ = -1;
    std::atomic<bool> running_{false};

    std::vector<std::thread> workers_;
    std::queue<int> clientQueue_;
    std::mutex queueMtx_;
    std::condition_variable queueCv_;
};
