#pragma once

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <future>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sw/redis++/redis++.h>

class redisClient
{
public:
    redisClient();
    redisClient(const std::string &host, int port, const std::string &password = "");

    bool LoadConfig(const std::string &path);
    bool Connect();

    // Redis commands (async)
    boost::asio::awaitable<void> set(const std::string &key, const std::string &value);
    boost::asio::awaitable<std::string> get(const std::string &key);

    boost::asio::awaitable<void> hset(const std::string &key, const std::string &field, const std::string &value);
    boost::asio::awaitable<std::string> hget(const std::string &key, const std::string &field);

    boost::asio::awaitable<void> zadd(const std::string &key, const std::string &member, double score);
    // boost::asio::awaitable<std::vector<std::pair<std::string, double>>> zrangeWithScores(const std::string &key, long start, long stop);
    boost::asio::awaitable<long long> exists(const std::string &key);
    boost::asio::awaitable<void> expire(const std::string &key, const int &expire);
    boost::asio::awaitable<void> hincrby(const std::string &key, const std::string &name, const int &number);

private:
    std::string host_;
    int port_;
    std::string password_;
    std::shared_ptr<sw::redis::Redis> redis_;
    std::shared_ptr<boost::asio::thread_pool> pool_;

    // Template helpers phải đặt trong header (nếu không sẽ lỗi undefined reference)
    template <typename Fn>
    boost::asio::awaitable<void> runBlocking(Fn fn)
    {
        auto exec = co_await boost::asio::this_coro::executor;
        auto pool = pool_; // copy shared_ptr

        boost::asio::post(*pool, [fn = std::move(fn), exec]() mutable
                          {
            try {
                fn();
            } catch (const std::exception &e) {
                std::cerr << "[Redis] Error in task: " << e.what() << std::endl;
            } });

        co_return;
    }

    template <typename T, typename Fn>
    boost::asio::awaitable<T> runBlockingWithResult(Fn fn)
    {
        auto exec = co_await boost::asio::this_coro::executor;
        auto pool = pool_;
        auto p = std::make_shared<std::promise<T>>();
        auto f = p->get_future();

        boost::asio::post(*pool, [fn = std::move(fn), p]() mutable
                          {
            try {
                p->set_value(fn());
            } catch (...) {
                try {
                    p->set_exception(std::current_exception());
                } catch (...) {}
            } });

        co_return f.get();
    }
};
