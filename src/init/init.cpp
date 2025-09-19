#include "init.h"
#include <iostream>
#include <cstdlib>
#include <curl/curl.h>
#include <sys/utsname.h>

using json = nlohmann::json;

// ------------------- CURL -------------------
static size_t WriteCallback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t totalSize = size * nmemb;
    if (userdata)
        reinterpret_cast<std::string *>(userdata)->append(reinterpret_cast<char *>(ptr), totalSize);
    return totalSize;
}

nlohmann::json init::fetchIpInfoSync()
{
    CURL *curl = curl_easy_init();
    json result;
    if (!curl)
        return result;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io/json");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res == CURLE_OK)
        result = json::parse(response, nullptr, false);
    return result;
}

// ------------------- Coroutine version -------------------
boost::asio::awaitable<void> init::initWhoAmI(redisClient &redisClient)
{
    // Không cần lấy io_context ở đây nữa

    struct utsname sysInfo;
    if (uname(&sysInfo) != 0)
    {
        perror("uname failed");
        co_return;
    }

    try
    {
        json locationInfo = fetchIpInfoSync();

        json whoAmI;
        whoAmI["name"] = std::getenv("NAME") ? std::getenv("NAME") : "default_server";
        whoAmI["ip"] = locationInfo.value("ip", "");
        whoAmI["city"] = locationInfo.value("city", "");
        whoAmI["country"] = locationInfo.value("country", "");
        whoAmI["region"] = locationInfo.value("region", "");
        whoAmI["arch"] = sysInfo.machine;
        whoAmI["quicPort"] = std::getenv("PORT") ? std::getenv("PORT") : "4443";
        whoAmI["platform"] = sysInfo.sysname;

        std::cout << "=== Server Info ===\n"
                  << whoAmI.dump(4) << std::endl;

        // Coroutine awaitable Redis hset
        co_await redisClient.hset("manage:server_list", whoAmI["name"], whoAmI.dump());
        std::cout << "Server_list saved OK" << std::endl;

        co_await redisClient.hset("manage:user_online_by_server", whoAmI["name"], "0");
        std::cout << "User_online_by_server saved OK" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in initWhoAmI: " << e.what() << std::endl;
    }
}

// ------------------- Load -------------------
bool init::Load()
{
    postgresClient pg;
    if (!pg.Connect())
        return false;

    redisClient redis;
    if (!redis.Connect())
        return false;

    boost::asio::io_context io;

    // chạy coroutine initWhoAmI
    boost::asio::co_spawn(io, [&redis, this]() -> boost::asio::awaitable<void>
                          { co_await initWhoAmI(redis); }, boost::asio::detached);

    io.run();
    return true;
}
