#pragma once
#include "quicServer.h"
#include "database/redis/redisClient.h"
#include <string>
#include <boost/asio/awaitable.hpp>

class managerService
{
public:
    explicit managerService(redisClient &redis);

    boost::asio::awaitable<void> addNewPlayerOnline();
    boost::asio::awaitable<void> removePlayerOnline();

private:
    redisClient &redis;
};
