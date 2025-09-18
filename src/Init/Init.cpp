#include "Init.h"
#include "../Database/Redis/RedisClient.h"
#include "../Database/Postgres/PostgresClient.h"
#include <iostream>
#include <string>
#include <cstdlib>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <sys/utsname.h>

using json = nlohmann::json;

// Hàm callback để libcurl ghi response vào string
static size_t WriteCallback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t totalSize = size * nmemb;
    if (userdata)
    {
        std::string *buffer = reinterpret_cast<std::string *>(userdata);
        buffer->append(reinterpret_cast<char *>(ptr), totalSize);
    }
    return totalSize; // Phải trả về đúng số byte nhận được
}

// Lấy JSON từ ipinfo.io
static bool fetchIpInfo(json &result)
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "https://ipinfo.io/json");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // tránh treo

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        std::cerr << "curl_easy_perform() failed: "
                  << curl_easy_strerror(res) << std::endl;
        return false;
    }

    result = json::parse(response, nullptr, false);
    if (result.is_discarded())
    {
        std::cerr << "Failed to parse JSON from ipinfo.io" << std::endl;
        return false;
    }
    return true;
}

void Init::initWhoAmI(RedisClient &redisClient)
{
    struct utsname sysInfo;
    if (uname(&sysInfo) != 0)
    {
        perror("uname failed");
        return;
    }

    try
    {
        // 1. Fetch IP/location info
        json locationInfo;
        if (!fetchIpInfo(locationInfo))
        {
            std::cerr << "Could not fetch public IP info" << std::endl;
            return;
        }

        // 2. Tạo object whoAmI
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

        // 3. Lưu vào Redis
        redisClient.hset("manage:server_list", whoAmI["name"], whoAmI.dump(),
                         [](bool ok)
                         {
                             if (!ok)
                             {
                                 std::cerr << "Failed to save server_list to Redis" << std::endl;
                             }
                             else
                             {
                                 std::cout << "Server_list saved OK" << std::endl;
                             }
                         });
        std::cout << "check async" << std::endl;
        redisClient.hset("manage:user_online_by_server", whoAmI["name"], "0",
                         [](bool ok)
                         {
                             if (!ok)
                             {
                                 std::cerr << "Failed to save user_online_by_server to Redis" << std::endl;
                             }
                             else
                             {
                                 std::cout << "User_online_by_server saved OK" << std::endl;
                             }
                         });
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error in initWhoAmI: " << e.what() << std::endl;
    }
}

bool Init::Load()
{
    PostgresClient pg;
    if (!pg.Connect())
        return false;

    RedisClient redis;
    if (!redis.Connect())
        return false;

    this->initWhoAmI(redis);
    return true;
}
