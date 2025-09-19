#include "userManager.h"
#include <iostream>
#include <nlohmann/json.hpp>

UserManager::UserManager(quicServer &server)
    : server_(server) {}

void UserManager::addUser(const User &user)
{
    if (hasUser(user.playerId))
    {
        return;
    }
    users_.emplace(user.playerId, user);
}

bool UserManager::hasUser(const std::string &playerId) const
{
    return users_.find(playerId) != users_.end();
}

void UserManager::removeUser(const std::string &playerId)
{
    auto it = users_.find(playerId);
    if (it == users_.end())
    {
        return;
    }
    users_.erase(it);
}

void UserManager::sendMessageToUser(const std::string &playerId, const std::string &data)
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

User *UserManager::getUser(const std::string &playerId)
{
    auto it = users_.find(playerId);
    if (it == users_.end())
    {
        return nullptr;
    }
    return &it->second;
}

void UserManager::broadcastToAll(const std::string &message)
{
    for (const auto &[pid, user] : users_)
    {
        server_.sendMessage(user.connId, message);
    }
}
