#pragma once
#include "quicServer.h"
#include <unordered_map>
#include <string>
#include <memory>

struct User
{
    uint64_t connId;
    std::string playerId; // định danh của user
    bool auth = false;
};

class UserManager
{
public:
    explicit UserManager(quicServer &server);

    void addUser(const User &user);
    bool hasUser(const std::string &playerId) const;
    void removeUser(const std::string &playerId);

    void sendMessageToUser(const std::string &playerId, const std::string &data);
    User *getUser(const std::string &playerId);

    void broadcastToAll(const std::string &message);

private:
    quicServer &server_;
    std::unordered_map<std::string, User> users_; // key = playerId
};
