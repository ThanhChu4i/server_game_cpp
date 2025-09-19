#include "userManager.h"
#include <iostream>
#include <nlohmann/json.hpp>

userManager::userManager(quicServer &server)
    : server_(server) {}

void userManager::addUser(const User &user)
{
    if (hasUser(user.playerId))
    {
        return;
    }
    users_.emplace(user.playerId, user);
}

bool userManager::hasUser(const std::string &playerId) const
{
    return users_.find(playerId) != users_.end();
}

void userManager::removeUser(const std::string &playerId)
{
    auto it = users_.find(playerId);
    if (it == users_.end())
    {
        return;
    }
    users_.erase(it);
}

void userManager::sendMessageToUser(const std::string &playerId, const std::string &data)
{
    auto it = users_.find(playerId);
    if (it == users_.end())
    {
        return;
    }
    const auto &user = it->second;
    if (!server_.sendMessage(user.connId, data))
    {
        std::cerr << "Failed to send message to user " << playerId << "\n";
    }
}

User *userManager::getUser(const std::string &playerId)
{
    auto it = users_.find(playerId);
    if (it == users_.end())
    {
        return nullptr;
    }
    return &it->second;
}

void userManager::broadcastToAll(const std::string &message)
{
    for (const auto &[pid, user] : users_)
    {
        server_.sendMessage(user.connId, message);
    }
}
