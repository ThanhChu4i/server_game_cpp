#include "../database/redis/redisClient.h"
#include "../database/postgres/postgresClient.h"
#include <iostream>
#include <nlohmann/json.hpp>
#include "userService.h"

using json = nlohmann::json;

// Khởi tạo global object RedisClient
redisClient redis;

constexpr int CACHE_EXPIRE = 3600 * 4;

// Service coroutine
boost::asio::awaitable<bool> userService::isExists(const std::string &playerUUID)
{
    try
    {
        std::string key = "player:" + playerUUID + ":info";
        auto count = co_await redis.exists(key);
        co_return (count > 0);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in isExists: " << e.what() << std::endl;
        co_return false;
    }
}

boost::asio::awaitable<bool> userService::updateRedisCache(const std::string &playerUUID, const json &data)
{
    try
    {
        std::string key = "player:" + playerUUID + ":info";

        // Serialize JSON thành string (hset thường lưu field-value)
        for (auto it = data.begin(); it != data.end(); ++it)
        {
            co_await redis.hset(key, it.key(), it.value().dump());
        }

        co_await redis.expire(key, CACHE_EXPIRE);
        co_return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in updateRedisCache: " << e.what() << std::endl;
        co_return false;
    }
}

// boost::asio::awaitable<bool> sendMessage(const std::string &playerUUID, const json &message)
// {
//     try
// }
boost::asio::awaitable<bool>