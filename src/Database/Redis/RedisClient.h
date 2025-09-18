#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>
#include <functional>
#include <memory>

class RedisClient
{
public:
    using BoolCallback = std::function<void(bool)>;
    using StringCallback = std::function<void(const std::string &)>;

    RedisClient();
    RedisClient(const std::string &host, int port, const std::string &password = "");
    ~RedisClient() = default;

    bool Connect();

    // Sync APIs
    std::string get(const std::string &key);
    bool set(const std::string &key, const std::string &value);
    bool hset(const std::string &hash, const std::string &field, const std::string &value);
    std::string hget(const std::string &hash, const std::string &field);
    bool zadd(const std::string &key, const std::string &member, double score);

    // Async (callback-style). Non-blocking, callback invoked when done.
    // NOTE: these schedule work on detached threads; callback runs on that worker thread.
    void getAsync(const std::string &key, StringCallback cb);
    void setAsync(const std::string &key, const std::string &value, BoolCallback cb);
    void hsetAsync(const std::string &hash, const std::string &field, const std::string &value, BoolCallback cb);
    void hgetAsync(const std::string &hash, const std::string &field, StringCallback cb);
    void zaddAsync(const std::string &key, const std::string &member, double score, BoolCallback cb);

private:
    bool LoadConfig(const std::string &path);

    std::string host_;
    int port_;
    std::string password_;

    // single smart pointer to redis connection
    std::shared_ptr<sw::redis::Redis> redis_;
};
