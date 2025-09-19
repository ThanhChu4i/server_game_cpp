#pragma once
#include "clientContext.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class MessageDispatcher
{
public:
    // Đăng ký handler cho mỗi event
    void RegisterHandler(const std::string &eventName,
                         std::function<void(ClientContext &, const json &)> handler)
    {
        handlers_[eventName] = handler;
    }

    // Gọi để xử lý message JSON từ client
    void Dispatch(ClientContext &client, const std::string &msgStr)
    {
        try
        {
            auto msg = json::parse(msgStr);

            // Tìm event có tồn tại trong msg
            for (auto &[eventName, handler] : handlers_)
            {
                if (msg.contains(eventName))
                {
                    handler(client, msg[eventName]);
                    return;
                }
            }

            // Không tìm thấy event hợp lệ
            client.sendMessage("{\"auth\":{\"action\":\"error\",\"data\":{\"message\":\"Unauthorized or unknown action\"}}}");
        }
        catch (const std::exception &e)
        {
            client.sendMessage("{\"auth\":{\"action\":\"error\",\"data\":{\"message\":\"Invalid message format\"}}}");
        }
    }

private:
    std::unordered_map<std::string, std::function<void(ClientContext &, const json &)>> handlers_;
};
