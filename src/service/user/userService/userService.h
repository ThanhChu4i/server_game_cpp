#pragma once
#include "../database/redis/redisClient.h"
#include "../database/postgres/postgresClient.h"
#include <nlohmann/json.hpp>
#include <boost/asio/awaitable.hpp>
#include <string>

using json = nlohmann::json;

class userService
{
public:
    explicit userService(redisClient &redis, postgresClient &pg);

    // Check user cache trong Redis
    boost::asio::awaitable<bool> isExists(const std::string &playerUUID);

    // Update cache với dữ liệu JSON
    boost::asio::awaitable<bool> updateRedisCache(const std::string &playerUUID, const json &data);

private:
    redisClient &redis;
    postgresClient &pg;
    static constexpr int CACHE_EXPIRE = 3600 * 4; // 4 giờ
};

#endif // USER_SERVICE_H