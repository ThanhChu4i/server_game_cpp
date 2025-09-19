#pragma once

#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>
#include "msquic.h"
#include <nlohmann/json.hpp>
#include "../quicServer/quicServer.h"

struct Player
{
    int x{0}, y{0};
    std::string name;
    HQUIC stream{nullptr};
    int score{0};
};

struct Item
{
    int id;
    int x, y;
    bool active{true};
};

struct Bullet
{
    int id;
    int x, y;
    int dx, dy;
    std::string shooter_name;
    bool active{true};
    int speed{5};
};

class Gameplay
{
public:
    explicit Gameplay(quicServer &server);
    ~Gameplay();

    // Called from QUIC callbacks via posted tasks (so thread-safety still needed)
    void handleMessage(HQUIC stream, const std::string &msg);
    void handlePlayerConnected(HQUIC conn, HQUIC stream);
    void handlePlayerDisconnected(HQUIC stream);

    void startGameLoop();
    void stopGameLoop();

private:
    quicServer &quic_server_;

    // game state
    std::map<HQUIC, Player> players_;
    std::mutex players_mutex_;

    std::vector<Item> items_;
    std::mutex items_mutex_;
    int nextItemId_{1};

    std::vector<Bullet> bullets_;
    std::mutex bullets_mutex_;
    int nextBulletId_{1};

    std::atomic<bool> gameRunning_{false};
    std::thread gameThread_;

    // internal helpers
    void gameLoop();
    void broadcastGameState();
    void addPlayer(HQUIC stream, const std::string &name);
    void removePlayer(HQUIC stream);
    void spawnItem();
    void checkItemCollection();
    void createBullet(const std::string &shooterName, int x, int y, int dx, int dy);
    void updateBullets();
    void checkBulletCollisions();
    void sendWelcomeMessage(HQUIC stream, const std::string &playerName);
};
