#pragma once
#include "../Database/Redis/RedisClient.h"
#include "../Coroutine/Task.h"

class Init
{
public:
    void initWhoAmI(RedisClient &redisClient);
    bool Load();
};
