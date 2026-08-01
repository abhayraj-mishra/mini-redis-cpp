#include "server.hpp"
#include "store.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 6380; // avoid clashing with real redis on 6379
    if (argc > 1) port = std::stoi(argv[1]);

    KVStore store;
    // store.loadFromLog("dump.aof"); // TODO(day3): enable once persistence is implemented

    Server server(port, store);
    server.run();

    return 0;
}
