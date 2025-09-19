#pragma once
#include "../database/redis/redisClient.h"
#include "../database/postgres/postgresClient.h"
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

class init
{
public:
    bool Load();

    // Coroutine version
    boost::asio::awaitable<void> initWhoAmI(redisClient &redisClient);

private:
    nlohmann::json fetchIpInfoSync();
};
