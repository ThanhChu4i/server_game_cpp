#include "database/redis/redisClient.h"
#include <iostream>
#include "managerService.h"

managerService::managerService(redisClient &redis)
    : redis(redis) {}

boost::asio::awaitable<void> managerService::addNewPlayerOnline()
{
    auto ok = co_await redis.hincrby("manage:user_online_by_server", "default_server", 1);
    co_return; // ✅ cần có co_return khi dùng coroutine
}

boost::asio::awaitable<void> managerService::removePlayerOnline()
{
    auto ok = co_await redis.hincrby("manage:user_online_by_server", "default_server", -1);
    co_return;
}
