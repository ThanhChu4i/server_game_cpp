#include "gameplay.h"
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include "../quicServer/quicServer.h"
using json = nlohmann::json;

Gameplay::Gameplay(quicServer &server, boost::asio::io_context &io)
    : quic_server_(server), gameLoopTimer_(io)
{
    srand(static_cast<unsigned int>(time(nullptr)));
    lastItemSpawn_ = std::chrono::steady_clock::now();
}

Gameplay::~Gameplay()
{
    stopGameLoop();
}

void Gameplay::handleMessage(HQUIC stream, const std::string &msg)
{
    try
    {
        std::cout << "[handleMessage] Received raw message:" << msg << std::endl;
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
            double dx = j.value("dx", 0.0);
            double dy = j.value("dy", 0.0);
            std::string shooterName = j.value("player", "");

            if (x >= 0 && y >= 0 && (dx != 0.0 || dy != 0.0) && !shooterName.empty())
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
    // Đợi client gửi tin nhắn "join" để thêm người chơi
}

void Gameplay::handlePlayerDisconnected(HQUIC stream)
{
    removePlayer(stream);
    std::cout << "[Gameplay] Player disconnected stream=" << stream << "\n";
}

void Gameplay::startGameLoop()
{
    gameRunning_ = true;
    gameLoop(boost::system::error_code()); // Bắt đầu vòng lặp đầu tiên
}

void Gameplay::stopGameLoop()
{
    gameRunning_ = false;
    gameLoopTimer_.cancel();
}

void Gameplay::gameLoop(const boost::system::error_code &error)
{
    auto now = std::chrono::steady_clock::now();

    if (error == boost::asio::error::operation_aborted)
        return;

    if (!gameRunning_)
        return;

    // Logic game
    checkItemCollection();
    updateBullets();
    checkBulletCollisions();

    if (now - lastItemSpawn_ >= std::chrono::seconds(5))
    {
        spawnItem();
        lastItemSpawn_ = now;
    }

    broadcastGameState();

    // Hẹn giờ lặp tiếp
    gameLoopTimer_.expires_at(gameLoopTimer_.expiry() + std::chrono::milliseconds(50));
    gameLoopTimer_.async_wait([this](const boost::system::error_code &e)
                              { gameLoop(e); });
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
        items_.erase(std::remove_if(items_.begin(), items_.end(), [](const Item &it)
                                    { return !it.active; }),
                     items_.end());
        itemSnapshot = items_;
    }

    std::vector<Bullet> bulletSnapshot;
    {
        std::lock_guard<std::mutex> lock(bullets_mutex_);
        bullets_.erase(std::remove_if(bullets_.begin(), bullets_.end(), [](const Bullet &b)
                                      { return !b.active; }),
                       bullets_.end());
        bulletSnapshot = bullets_;
    }

    // Gửi dữ liệu trạng thái
    json state_json = json::object();
    state_json["players"] = json::array();
    for (const auto &p : playerSnapshot)
        state_json["players"].push_back({{"name", p.name}, {"x", p.x}, {"y", p.y}, {"score", p.score}});

    state_json["items"] = json::array();
    for (const auto &it : itemSnapshot)
        state_json["items"].push_back({{"id", it.id}, {"x", it.x}, {"y", it.y}});

    state_json["bullets"] = json::array();
    for (const auto &b : bulletSnapshot)
        state_json["bullets"].push_back({{"id", b.id}, {"x", b.x}, {"y", b.y}, {"dx", b.dx}, {"dy", b.dy}, {"shooter", b.shooter_name}});

    std::string msg = state_json.dump() + "\n";

    for (auto &stream : playerStreams)
    {
        quic_server_.sendMessage(stream, msg);
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
        if (b.x < 0 || b.x > 2000 || b.y < 0 || b.y > 2000)
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
    {
        nlohmann::json j;
        j["action"] = "welcome";
        j["player"] = playerName;

        std::string msg = j.dump() + "\n"; // ensure newline separator
        quic_server_.sendMessage(stream, msg);
    }
}