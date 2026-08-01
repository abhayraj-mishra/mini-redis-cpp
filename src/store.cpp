#include "store.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

void KVStore::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key] = value;
    expiries_.erase(key); // clear any old TTL on overwrite
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto expIt = expiries_.find(key);
    if (expIt != expiries_.end() && std::chrono::steady_clock::now() > expIt->second) {
        data_.erase(key);
        expiries_.erase(expIt);
        return std::nullopt;
    }

    auto it = data_.find(key);
    if (it == data_.end()) return std::nullopt;
    return it->second;
}

bool KVStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    expiries_.erase(key);
    return data_.erase(key) > 0;
}

bool KVStore::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.find(key) != data_.end();
}

void KVStore::expire(const std::string& key, int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (data_.find(key) == data_.end()) return;
    expiries_[key] = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
}

void KVStore::appendToLog(const std::string& line) {
    // TODO(day3): open in append mode, write line, flush.
    // Keep this cheap -- called on every write command.
    (void)line;
}

void KVStore::loadFromLog(const std::string& path) {
    // TODO(day3): read file line by line, parse "SET k v" / "DEL k",
    // and replay into this->set()/this->del() to rebuild state on startup.
    (void)path;
}
