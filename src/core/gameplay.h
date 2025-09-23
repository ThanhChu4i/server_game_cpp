#ifndef GAMEPLAY_H
#define GAMEPLAY_H

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>
#include <chrono>
#include <cmath>
#include <boost/asio/steady_timer.hpp>
#include "../quicServer/quicServer.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Cấu trúc đại diện cho một người chơi
struct Player
{
    std::string name;
    int x, y;
    int score = 0;
    HQUIC stream = nullptr;
};

// Cấu trúc đại diện cho một vật phẩm
struct Item
{
    uint32_t id;
    int x, y;
    bool active = true;
};

// Cấu trúc đại diện cho một viên đạn
struct Bullet
{
    uint32_t id;
    int x, y;
    double dx, dy;
    std::string shooter_name;
    bool active = true;
    double speed = 15.0;
};

class quicServer;
class Gameplay
{
public:
    // Thêm io_context vào hàm tạo để sử dụng timer bất đồng bộ
    Gameplay(quicServer &server, boost::asio::io_context &io);
    ~Gameplay();
    // Xử lý các tin nhắn đến từ client
    void handleMessage(HQUIC stream, const std::string &msg);
    // Xử lý khi có stream mới được tạo
    void handlePlayerConnected(HQUIC conn, HQUIC stream);
    // Xử lý khi người chơi ngắt kết nối
    void handlePlayerDisconnected(HQUIC stream);

    // Bắt đầu và dừng vòng lặp game
    void startGameLoop();
    void stopGameLoop();
    // Các hàm logic game

private:
    quicServer &quic_server_;

    std::map<HQUIC, Player> players_;
    std::mutex players_mutex_;

    std::vector<Item> items_;
    std::mutex items_mutex_;

    std::vector<Bullet> bullets_;
    std::mutex bullets_mutex_;

    // Các ID duy nhất
    std::atomic<uint32_t> nextItemId_{1};
    std::atomic<uint32_t> nextBulletId_{1};
    std::chrono::steady_clock::time_point lastItemSpawn_;
    // Vòng lặp game bất đồng bộ
    boost::asio::steady_timer gameLoopTimer_;
    std::atomic<bool> gameRunning_{false};
    void gameLoop(const boost::system::error_code &error);
    void addPlayer(HQUIC stream, const std::string &name);
    void removePlayer(HQUIC stream);
    void broadcastGameState();
    void spawnItem();
    void createBullet(const std::string &shooterName, int x, int y, int dx, int dy);
    void checkItemCollection();
    void updateBullets();
    void checkBulletCollisions();
    void sendWelcomeMessage(HQUIC stream, const std::string &playerName);
};

#endif // GAMEPLAY_H