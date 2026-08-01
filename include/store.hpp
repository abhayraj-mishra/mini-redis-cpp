#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <vector>

// A thread-safe in-memory key-value store with optional TTL expiry.
// TODO(day2): shard this into N buckets, each with its own mutex,
//             to reduce lock contention under many concurrent clients.
class KVStore {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);

    // TODO(day2): implement TTL properly.
    // Store an expiry timestamp alongside the value and have a background
    // thread sweep expired keys periodically (see reaper thread in server.hpp).
    void expire(const std::string& key, int seconds);

    // TODO(day3): implement AOF persistence.
    // Every successful set/del should be appended as a line to a log file,
    // e.g. "SET key value\n" / "DEL key\n". On startup, replay the log
    // line-by-line into this store before starting the server loop.
    void appendToLog(const std::string& line);
    void loadFromLog(const std::string& path);

private:
    std::unordered_map<std::string, std::string> data_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiries_;
    std::mutex mutex_;
};
