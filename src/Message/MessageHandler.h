#pragma once
#include "ClientContext.h"
#include "MessageDispatcher.h"
#include <nlohmann/json.hpp>
#include <functional>

using json = nlohmann::json;

class MessageHandler
{
public:
    MessageHandler(MessageDispatcher &dispatcher)
    {
        // Đăng ký tất cả handler ở đây
        dispatcher.RegisterHandler("auth", std::bind(&MessageHandler::HandleAuth, this, std::placeholders::_1, std::placeholders::_2));
        dispatcher.RegisterHandler("guild", std::bind(&MessageHandler::HandleGuild, this, std::placeholders::_1, std::placeholders::_2));
        dispatcher.RegisterHandler("chat", std::bind(&MessageHandler::HandleChat, this, std::placeholders::_1, std::placeholders::_2));
        // ... thêm các handler khác
    }

    // Các handler
    void HandleAuth(ClientContext &client, const json &data)
    {
        // Ví dụ auth
        if (data.contains("token") && data["token"] == "123")
        {
            client.auth = true;
            client.playerId = data.value("playerId", "unknown");
            client.sendMessage("{\"auth\":{\"action\":\"success\"}}");
        }
        else
        {
            client.sendMessage("{\"auth\":{\"action\":\"error\",\"data\":{\"message\":\"Invalid token\"}}}");
        }
    }

    void HandleGuild(ClientContext &client, const json &data)
    {
        if (!client.auth)
        {
            client.sendMessage("{\"guild\":{\"action\":\"error\",\"data\":{\"message\":\"Unauthorized\"}}}");
            return;
        }
        client.sendMessage("{\"guild\":{\"action\":\"success\",\"data\":\"Guild handled\"}}");
    }

    void HandleChat(ClientContext &client, const json &data)
    {
        if (!client.auth)
            return;
        // Xử lý chat message
        client.sendMessage("{\"chat\":{\"action\":\"echo\",\"data\":\"" + data.dump() + "\"}}");
    }

    // ... các handler khác
};
