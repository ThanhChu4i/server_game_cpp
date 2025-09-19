#include "redisClient.h"
#include <iostream>
#include <fstream>

// ---------------- Constructor ----------------
redisClient::redisClient()
    : host_("127.0.0.1"), port_(6379), password_(), redis_(nullptr),
      pool_(std::make_shared<boost::asio::thread_pool>(4)) // 4 thread worker
{
    if (!LoadConfig("database/redis/config.json"))
    {
        std::cerr << "Failed to load Redis config, using defaults!" << std::endl;
    }
}

redisClient::redisClient(const std::string &host, int port, const std::string &password)
    : host_(host), port_(port), password_(password), redis_(nullptr),
      pool_(std::make_shared<boost::asio::thread_pool>(4)) {}

// ---------------- Config & Connect ----------------
bool redisClient::LoadConfig(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    try
    {
        nlohmann::json j;
        file >> j;
        host_ = j.value("host", host_);
        port_ = j.value("port", port_);
        password_ = j.value("password", password_);
        std::cout << "Redis config loaded: host=" << host_ << " port=" << port_ << std::endl;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool redisClient::Connect()
{
    try
    {
        sw::redis::ConnectionOptions opts;
        opts.host = host_;
        opts.port = port_;
        if (!password_.empty())
            opts.password = password_;
        redis_ = std::make_shared<sw::redis::Redis>(opts);
        std::cout << "Connected to Redis successfully!" << std::endl;
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Redis connection failed: " << e.what() << std::endl;
        redis_.reset();
        return false;
    }
}

// ---------------- Redis Commands ----------------

boost::asio::awaitable<void> redisClient::set(const std::string &key, const std::string &value)
{
    co_await runBlocking([this, key, value]()
                         { redis_->set(key, value); });
}

boost::asio::awaitable<std::string> redisClient::get(const std::string &key)
{
    co_return co_await runBlockingWithResult<std::string>([this, key]() -> std::string
                                                          {
        auto val = redis_->get(key);
        return val.value_or(""); });
}

boost::asio::awaitable<void> redisClient::hset(const std::string &key, const std::string &field, const std::string &value)
{
    co_await runBlocking([this, key, field, value]()
                         { redis_->hset(key, field, value); });
}

boost::asio::awaitable<std::string> redisClient::hget(const std::string &key, const std::string &field)
{
    co_return co_await runBlockingWithResult<std::string>([this, key, field]() -> std::string
                                                          {
        auto val = redis_->hget(key, field);
        return val.value_or(""); });
}

boost::asio::awaitable<void> redisClient::zadd(const std::string &key, const std::string &member, double score)
{
    co_await runBlocking([this, key, member, score]()
                         { redis_->zadd(key, member, score); });
}
boost::asio::awaitable<long long> redisClient::exists(const std::string &key)
{
    co_return co_await runBlockingWithResult<long long>([this, key]() -> long long
                                                        { return redis_->exists(key); });
}
boost::asio::awaitable<void> redisClient::expire(const std::string &key, const int &expire)
{
    co_await runBlocking([this, key, expire]()
                         { 
                             bool ok = redis_->expire(key, expire);
                             if (!ok) {
                                 std::cerr << "[Redis] expire failed for key: " << key << std::endl;
                             } });
}
// boost::asio::awaitable<std::vector<std::pair<std::string, double>>> redisClient::zrangeWithScores(const std::string &key, long start, long stop)
// {
//     co_return co_await runBlockingWithResult<std::vector<std::pair<std::string, double>>>(
//         [this, key, start, stop]()
//         {
//             std::vector<std::pair<std::string, double>> result;
//             redis_->zrange(key, start, stop, std::back_inserter(result), true);
//             return result;
//         });
// }
