#pragma once
#include "quicServer.h" // cần include để biết class quicServer
#include <unordered_map>
#include <string>

struct User
{
    uint64_t connId;
    std::string playerId;
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
    quicServer &server_; // tham chiếu đến quicServer
    std::unordered_map<std::string, User> users_;
};
