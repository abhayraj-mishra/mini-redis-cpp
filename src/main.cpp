#include "server.hpp"
#include "store.hpp"
#include <iostream>
#include <csignal>

namespace {
Server* g_server = nullptr;
extern "C" void onSignal(int) { if (g_server) g_server->stop(); }
}

int main(int argc, char* argv[]) {
    int port = 6380;
    if (argc > 1) port = std::stoi(argv[1]);

    KVStore store;
    store.loadFromLog("dump.aof");
    store.startReaper();

    Server server(port, store);
    g_server = &server;

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    server.run();
    return 0;
}
