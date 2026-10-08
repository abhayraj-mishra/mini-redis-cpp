#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <optional>
#include <array>
#include <fstream>
#include <thread>
#include <atomic>

class KVStore {
public:
    static constexpr size_t kShards = 16;

    KVStore();
    ~KVStore();

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    void expire(const std::string& key, int seconds);

    void appendToLog(const std::string& line);
    void loadFromLog(const std::string& path);

    void startReaper(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    void stopReaper();

private:
    struct Shard {
        std::unordered_map<std::string, std::string> data;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiries;
        mutable std::mutex mtx;
    };

    std::array<Shard, kShards> shards_;
    Shard& shardFor(const std::string& key);

    std::mutex logMtx_;
    std::ofstream logFile_;
    std::string logPath_ = "dump.aof";
    size_t logWritesSinceFlush_ = 0;

    std::thread reaper_;
    std::atomic<bool> stopReaper_{false};
    void reapExpired();
};
