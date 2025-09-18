#include "RedisClient.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <sstream>

using namespace sw::redis;

RedisClient::RedisClient()
    : host_("127.0.0.1"), port_(6379), password_(), redis_(nullptr)
{
    if (!LoadConfig("Database/Redis/config.json"))
    {
        std::cerr << "Failed to load Redis config, using defaults!" << std::endl;
    }
}

RedisClient::RedisClient(const std::string &host, int port, const std::string &password)
    : host_(host), port_(port), password_(password), redis_(nullptr)
{
}

bool RedisClient::LoadConfig(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Cannot open Redis config file: " << path << std::endl;
        return false;
    }

    try
    {
        nlohmann::json j;
        file >> j;
        host_ = j.value("host", host_);
        port_ = j.value("port", port_);
        password_ = j.value("password", password_);
        std::cout << "Redis config loaded: host=" << host_ << " port=" << port_ << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Redis config parse error: " << e.what() << std::endl;
        return false;
    }
    return true;
}

bool RedisClient::Connect()
{
    try
    {
        sw::redis::ConnectionOptions opts;
        opts.host = host_;
        opts.port = port_;
        if (!password_.empty())
            opts.password = password_;

        // create the shared_ptr once
        redis_ = std::make_shared<sw::redis::Redis>(opts);

        std::cout << "Connected to Redis at " << host_ << ":" << port_ << " successfully!" << std::endl;
        return true;
    }
    catch (const sw::redis::Error &e)
    {
        std::cerr << "Redis connection error: " << e.what() << std::endl;
        redis_.reset();
        return false;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Redis unknown error: " << e.what() << std::endl;
        redis_.reset();
        return false;
    }
}

// ---------------- Async (callback) implementations ----------------
void RedisClient::get(const std::string &key, StringCallback cb)
{
    auto redis_copy = redis_;
    if (!redis_copy)
    {
        if (cb)
            cb(std::string());
        return;
    }

    std::thread([redis_copy, key, cb]()
                {
        try {
            auto val = redis_copy->get(key);
            if (cb) cb(val ? *val : std::string());
        } catch (const sw::redis::Error &e) {
            std::cerr << "GET error (async): " << e.what() << std::endl;
            if (cb) cb(std::string());
        } })
        .detach();
}

void RedisClient::set(const std::string &key, const std::string &value, BoolCallback cb)
{
    auto redis_copy = redis_;
    if (!redis_copy)
    {
        if (cb)
            cb(false);
        return;
    }

    std::thread([redis_copy, key, value, cb]()
                {
        try {
            redis_copy->set(key, value);
            if (cb) cb(true);
        } catch (const sw::redis::Error &e) {
            std::cerr << "SET error (async): " << e.what() << std::endl;
            if (cb) cb(false);
        } })
        .detach();
}

void RedisClient::hset(const std::string &hash, const std::string &field, const std::string &value, BoolCallback cb)
{
    auto redis_copy = redis_;
    if (!redis_copy)
    {
        if (cb)
            cb(false);
        return;
    }

    std::thread([redis_copy, hash, field, value, cb]()
                {
        try {
            redis_copy->hset(hash, field, value);
            if (cb) cb(true);
        } catch (const sw::redis::Error &e) {
            std::cerr << "HSET error (async): " << e.what() << std::endl;
            if (cb) cb(false);
        } })
        .detach();
}

void RedisClient::hget(const std::string &hash, const std::string &field, StringCallback cb)
{
    auto redis_copy = redis_;
    if (!redis_copy)
    {
        if (cb)
            cb(std::string());
        return;
    }

    std::thread([redis_copy, hash, field, cb]()
                {
        try {
            auto val = redis_copy->hget(hash, field);
            if (cb) cb(val ? *val : std::string());
        } catch (const sw::redis::Error &e) {
            std::cerr << "HGET error (async): " << e.what() << std::endl;
            if (cb) cb(std::string());
        } })
        .detach();
}

void RedisClient::zadd(const std::string &key, const std::string &member, double score, BoolCallback cb)
{
    auto redis_copy = redis_;
    if (!redis_copy)
    {
        if (cb)
            cb(false);
        return;
    }

    std::thread([redis_copy, key, member, score, cb]()
                {
        try {
            redis_copy->zadd(key, member, score);
            if (cb) cb(true);
        } catch (const sw::redis::Error &e) {
            std::cerr << "ZADD error (async): " << e.what() << std::endl;
            if (cb) cb(false);
        } })
        .detach();
}
