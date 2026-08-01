#pragma once
#include "store.hpp"
#include <string>

// A simple blocking TCP server: one thread per connected client.
// Protocol: plain text, one command per line, e.g.
//   SET foo bar
//   GET foo
//   DEL foo
//   EXISTS foo
//   EXPIRE foo 30
//
// TODO(day2): swap "thread per client" for a fixed-size thread pool so
//             the server doesn't fall over under thousands of connections.
// TODO(day4): add PUBLISH/SUBSCRIBE -- maintain a map<channel, vector<socket_fd>>
//             and write to all subscribed sockets when PUBLISH is called.
class Server {
public:
    Server(int port, KVStore& store);
    void run();

private:
    void handleClient(int clientSocket);
    std::string processCommand(const std::string& line);

    int port_;
    KVStore& store_;
};
