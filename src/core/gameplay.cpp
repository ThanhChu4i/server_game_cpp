#include "gameplay.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cmath>

using json = nlohmann::json;

Gameplay::Gameplay(quicServer &server) : quic_server_(server)
{
    srand(static_cast<unsigned int>(time(nullptr)));
}

Gameplay::~Gameplay()
{
    stopGameLoop();
    if (gameThread_.joinable())
        gameThread_.join();
}

void Gameplay::handleMessage(HQUIC stream, const std::string &msg)
{
    try
    {
        auto j = json::parse(msg);
        std::string action = j.value("action", "");

        if (action == "join")
        {
            std::string playerName = j.value("player", "");
            addPlayer(stream, playerName);
        }
        else if (action == "move")
        {
            int newX = j.value("x", -1);
            int newY = j.value("y", -1);
            std::lock_guard<std::mutex> lock(players_mutex_);
            auto it = players_.find(stream);
            if (it != players_.end())
            {
                if (newX >= 0 && newY >= 0)
                {
                    it->second.x = newX;
                    it->second.y = newY;
                }
            }
        }
        else if (action == "shoot")
        {
            int x = j.value("x", -1);
            int y = j.value("y", -1);
            int dx = j.value("dx", 0);
            int dy = j.value("dy", 0);
            std::string shooterName = j.value("player", "");

            if (x >= 0 && y >= 0 && (dx != 0 || dy != 0) && !shooterName.empty())
            {
                createBullet(shooterName, x, y, dx, dy);
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "[Gameplay] JSON parse error: " << e.what() << " msg=" << msg << "\n";
    }
}

void Gameplay::handlePlayerConnected(HQUIC /*conn*/, HQUIC stream)
{
    std::cout << "[Gameplay] Player stream started: " << stream << "\n";
    // Wait for client to send "join" message to actually add user
}

void Gameplay::handlePlayerDisconnected(HQUIC stream)
{
    removePlayer(stream);
    std::cout << "[Gameplay] Player disconnected stream=" << stream << "\n";
}

void Gameplay::startGameLoop()
{
    bool expected = false;
    if (gameRunning_.compare_exchange_strong(expected, true))
    {
        gameThread_ = std::thread(&Gameplay::gameLoop, this);
    }
}

void Gameplay::stopGameLoop()
{
    gameRunning_.store(false);
    if (gameThread_.joinable())
    {
        gameThread_.join();
    }
}

void Gameplay::gameLoop()
{
    auto lastItemSpawn = std::chrono::steady_clock::now();
    const auto itemSpawnInterval = std::chrono::seconds(5);

    while (gameRunning_.load())
    {
        auto now = std::chrono::steady_clock::now();

        checkItemCollection();
        updateBullets();
        checkBulletCollisions();

        if (now - lastItemSpawn >= itemSpawnInterval)
        {
            spawnItem();
            lastItemSpawn = now;
        }

        broadcastGameState();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void Gameplay::broadcastGameState()
{
    std::vector<Player> playerSnapshot;
    std::vector<HQUIC> playerStreams;

    {
        std::lock_guard<std::mutex> lock(players_mutex_);
        for (auto &kv : players_)
        {
            playerSnapshot.push_back(kv.second);
            playerStreams.push_back(kv.first);
        }
    }

    if (playerSnapshot.empty())
        return;

    std::vector<Item> itemSnapshot;
    {
        std::lock_guard<std::mutex> lock(items_mutex_);
        for (auto &it : items_)
            if (it.active)
                itemSnapshot.push_back(it);
    }

    std::vector<Bullet> bulletSnapshot;
    {
        std::lock_guard<std::mutex> lock(bullets_mutex_);
        for (auto &b : bullets_)
            if (b.active)
                bulletSnapshot.push_back(b);
    }

    json state_json = json::object();
    state_json["players"] = json::array();
    for (auto &p : playerSnapshot)
        state_json["players"].push_back({{"name", p.name}, {"x", p.x}, {"y", p.y}, {"score", p.score}});

    state_json["items"] = json::array();
    for (auto &it : itemSnapshot)
        state_json["items"].push_back({{"id", it.id}, {"x", it.x}, {"y", it.y}});

    state_json["bullets"] = json::array();
    for (auto &b : bulletSnapshot)
        state_json["bullets"].push_back({{"id", b.id}, {"x", b.x}, {"y", b.y}, {"dx", b.dx}, {"dy", b.dy}, {"shooter", b.shooter_name}});

    std::string msg = state_json.dump() + "\n";

    // Send to streams, but verify stream still exists in server's Clients map to avoid race.
    for (auto &stream : playerStreams)
    {
        // naive check: sendMessage returns false if it fails (e.g. closed)
        if (!quic_server_.sendMessage(stream, msg))
        {
            // optionally remove player if send fails
            removePlayer(stream);
        }
    }
}

void Gameplay::addPlayer(HQUIC stream, const std::string &name)
{
    std::lock_guard<std::mutex> lock(players_mutex_);
    if (players_.find(stream) != players_.end())
        return;
    Player p;
    p.x = 50 + (rand() % 500);
    p.y = 50 + (rand() % 300);
    p.name = name.empty() ? ("P" + std::to_string(nextItemId_++)) : name;
    p.stream = stream;
    players_.emplace(stream, std::move(p));
    std::cout << "[Gameplay] Added player " << players_[stream].name << "\n";
    sendWelcomeMessage(stream, players_[stream].name);
}

void Gameplay::removePlayer(HQUIC stream)
{
    std::lock_guard<std::mutex> lock(players_mutex_);
    auto it = players_.find(stream);
    if (it != players_.end())
    {
        std::cout << "[Gameplay] Removing player " << it->second.name << "\n";
        players_.erase(it);
    }
}

void Gameplay::spawnItem()
{
    std::lock_guard<std::mutex> lock(items_mutex_);
    Item it;
    it.id = nextItemId_++;
    it.x = 50 + (rand() % 500);
    it.y = 50 + (rand() % 300);
    it.active = true;
    items_.push_back(it);
    std::cout << "[Gameplay] Spawned item " << it.id << "\n";
}

void Gameplay::checkItemCollection()
{
    std::lock_guard<std::mutex> playersLock(players_mutex_);
    std::lock_guard<std::mutex> itemsLock(items_mutex_);

    for (auto &pkv : players_)
    {
        for (auto &it : items_)
        {
            if (!it.active)
                continue;
            int dx = pkv.second.x - it.x;
            int dy = pkv.second.y - it.y;
            int dist = static_cast<int>(std::sqrt(dx * dx + dy * dy));
            if (dist < 20)
            {
                it.active = false;
                pkv.second.score += 1;
                std::cout << "[Gameplay] Player " << pkv.second.name << " collected " << it.id << "\n";
            }
        }
    }
}

void Gameplay::createBullet(const std::string &shooterName, int x, int y, int dx, int dy)
{
    std::lock_guard<std::mutex> lock(bullets_mutex_);
    Bullet b;
    b.id = nextBulletId_++;
    b.x = x;
    b.y = y;
    b.dx = dx;
    b.dy = dy;
    b.shooter_name = shooterName;
    b.active = true;
    bullets_.push_back(b);
    std::cout << "[Gameplay] Bullet " << b.id << " from " << shooterName << "\n";
}

void Gameplay::updateBullets()
{
    std::lock_guard<std::mutex> lock(bullets_mutex_);
    for (auto &b : bullets_)
    {
        if (!b.active)
            continue;
        b.x += b.dx * b.speed;
        b.y += b.dy * b.speed;
        if (b.x < 0 || b.x > 2000 || b.y < 0 || b.y > 2000) // larger bounds
            b.active = false;
    }
}

void Gameplay::checkBulletCollisions()
{
    std::lock_guard<std::mutex> playersLock(players_mutex_);
    std::lock_guard<std::mutex> bulletsLock(bullets_mutex_);

    for (auto &b : bullets_)
    {
        if (!b.active)
            continue;
        for (auto &pkv : players_)
        {
            if (pkv.second.name == b.shooter_name)
                continue;
            int dx = pkv.second.x - b.x;
            int dy = pkv.second.y - b.y;
            int dist = static_cast<int>(std::sqrt(dx * dx + dy * dy));
            if (dist < 15)
            {
                b.active = false;
                pkv.second.score = std::max(0, pkv.second.score - 1);
                std::cout << "[Gameplay] Player " << pkv.second.name << " hit by " << b.shooter_name << "\n";
                break;
            }
        }
    }
}

void Gameplay::sendWelcomeMessage(HQUIC stream, const std::string &playerName)
{
    std::string msg = "WELCOME:" + playerName + "\n";
    quic_server_.sendMessage(stream, msg);
}
