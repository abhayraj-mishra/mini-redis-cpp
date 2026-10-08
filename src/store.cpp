#include "store.hpp"
#include <fstream>
#include <sstream>
#include <functional>

KVStore::KVStore() {
    logFile_.open(logPath_, std::ios::app);
}

KVStore::~KVStore() {
    stopReaper();
    if (logFile_.is_open()) logFile_.flush();
}

KVStore::Shard& KVStore::shardFor(const std::string& key) {
    static const std::hash<std::string> hasher{};
    return shards_[hasher(key) % kShards];
}

void KVStore::set(const std::string& key, const std::string& value) {
    {
        auto& s = shardFor(key);
        std::lock_guard<std::mutex> lock(s.mtx);
        s.data[key] = value;
        s.expiries.erase(key);
    }
    appendToLog("SET " + key + " " + value);
}

std::optional<std::string> KVStore::get(const std::string& key) {
    auto& s = shardFor(key);
    std::lock_guard<std::mutex> lock(s.mtx);

    auto expIt = s.expiries.find(key);
    if (expIt != s.expiries.end() && std::chrono::steady_clock::now() > expIt->second) {
        s.data.erase(key);
        s.expiries.erase(expIt);
        return std::nullopt;
    }
    auto it = s.data.find(key);
    if (it == s.data.end()) return std::nullopt;
    return it->second;
}

bool KVStore::del(const std::string& key) {
    bool removed = false;
    {
        auto& s = shardFor(key);
        std::lock_guard<std::mutex> lock(s.mtx);
        s.expiries.erase(key);
        removed = s.data.erase(key) > 0;
    }
    if (removed) appendToLog("DEL " + key);
    return removed;
}

bool KVStore::exists(const std::string& key) {
    return get(key).has_value();
}

void KVStore::expire(const std::string& key, int seconds) {
    auto& s = shardFor(key);
    std::lock_guard<std::mutex> lock(s.mtx);
    if (s.data.find(key) == s.data.end()) return;
    s.expiries[key] = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
}

void KVStore::appendToLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(logMtx_);
    if (!logFile_.is_open()) return;
    logFile_ << line << '\n';
    if (++logWritesSinceFlush_ >= 1000) {
        logFile_.flush();
        logWritesSinceFlush_ = 0;
    }
}

void KVStore::loadFromLog(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string cmd, key, value;
        iss >> cmd >> key;
        if (cmd == "SET") {
            std::getline(iss, value);
            if (!value.empty() && value[0] == ' ') value.erase(0, 1);
            set(key, value);
        } else if (cmd == "DEL") {
            del(key);
        }
    }
}

void KVStore::startReaper(std::chrono::milliseconds interval) {
    reaper_ = std::thread([this, interval] {
        while (!stopReaper_.load()) {
            std::this_thread::sleep_for(interval);
            if (stopReaper_.load()) break;
            reapExpired();
        }
    });
}

void KVStore::stopReaper() {
    stopReaper_.store(true);
    if (reaper_.joinable()) reaper_.join();
}

void KVStore::reapExpired() {
    auto now = std::chrono::steady_clock::now();
    for (auto& s : shards_) {
        std::lock_guard<std::mutex> lock(s.mtx);
        for (auto it = s.expiries.begin(); it != s.expiries.end(); ) {
            if (now > it->second) {
                s.data.erase(it->first);
                it = s.expiries.erase(it);
            } else {
                ++it;
            }
        }
    }
}
