#pragma once
#include <string>
#include <cstdint>

struct ClientContext
{
    uint64_t clientId;
    bool auth = false;
    std::string countryCode = "US";
    std::string userId;
    std::string playerId;

    // Hàm gửi message về client
    std::function<void(const std::string &)> sendMessage;

    // Push notification
    void pushNotification(const std::string &title, const std::string &message)
    {
        sendMessage("{\"notification\":{\"action\":\"notification\",\"data\":{\"title\":\"" + title + "\",\"message\":\"" + message + "\"}}}");
    }
};
