#include "gameplay.h"
#include "quicServer.h" // For MsQuic pointer and g_ServerRunning
#include <iostream>
#include <chrono>
#include <cmath>   // For sqrt
#include <cstdlib> // For rand, srand
#include <ctime>   // For time

// -------------------- Global Game State --------------------
std::map<HQUIC, Player> g_Players;
std::mutex g_PlayersMutex;

std::vector<Item> g_Items;
std::mutex g_ItemsMutex;
int g_NextItemId = 1;

std::vector<Bullet> g_Bullets;
std::mutex g_BulletsMutex;
int g_NextBulletId = 1;

int g_NextPlayerId = 1;

// -------------------- Item Management --------------------
static void SpawnItem()
{
    std::lock_guard<std::mutex> lock(g_ItemsMutex);
    Item newItem;
    newItem.id = g_NextItemId++;
    // Use a fixed game area for spawning
    newItem.x = 50 + (rand() % 500); // Random position in game area (e.g., 600 width)
    newItem.y = 50 + (rand() % 300); // Random position in game area (e.g., 400 height)
    newItem.active = true;
    g_Items.push_back(newItem);
    std::cout << "[Item] Spawned item " << newItem.id << " at (" << newItem.x << "," << newItem.y << ")\n";
}

static void CheckItemCollection()
{
    std::lock_guard<std::mutex> playersLock(g_PlayersMutex);
    std::lock_guard<std::mutex> itemsLock(g_ItemsMutex);

    // Use a temporary list for items to remove
    std::vector<int> collectedItemIds;

    for (auto &player_pair : g_Players)
    {
        Player &player = player_pair.second;
        for (auto &item : g_Items)
        {
            if (!item.active)
                continue;

            int dx = player.x - item.x;
            int dy = player.y - item.y;
            int distance = (int)sqrt(dx * dx + dy * dy);

            if (distance < 20)
            { // Collection radius
                item.active = false;
                player.score += 1;
                collectedItemIds.push_back(item.id);
                std::cout << "[Item] Player " << player.name
                          << " collected item " << item.id
                          << " (score: " << player.score << ")\n";
            }
        }
    }
    // Remove inactive items to prevent memory bloat
    g_Items.erase(std::remove_if(g_Items.begin(), g_Items.end(),
                                 [](const Item &item)
                                 { return !item.active; }),
                  g_Items.end());
}

// -------------------- Bullet Management --------------------
static void CreateBullet(const std::string &shooterName, int x, int y, int dx, int dy)
{
    std::lock_guard<std::mutex> lock(g_BulletsMutex);
    Bullet newBullet;
    newBullet.id = g_NextBulletId++;
    newBullet.x = x;
    newBullet.y = y;
    newBullet.dx = dx;
    newBullet.dy = dy;
    newBullet.shooter_name = shooterName;
    newBullet.active = true;
    g_Bullets.push_back(newBullet);
    std::cout << "[Bullet] Player " << shooterName << " fired bullet " << newBullet.id
              << " from (" << x << "," << y << ") direction (" << dx << "," << dy << ")\n";
}

static void UpdateBullets()
{
    std::lock_guard<std::mutex> lock(g_BulletsMutex);

    for (auto &bullet : g_Bullets)
    {
        if (!bullet.active)
            continue;

        // Move bullet
        bullet.x += bullet.dx * bullet.speed;
        bullet.y += bullet.dy * bullet.speed;

        // Remove bullet if out of bounds (using sample game area 0-600, 0-400)
        if (bullet.x < -50 || bullet.x > 650 || bullet.y < -50 || bullet.y > 450)
        { // Add some padding
            bullet.active = false;
            std::cout << "[Bullet] Bullet " << bullet.id << " out of bounds\n";
        }
    }
    // Remove inactive bullets
    g_Bullets.erase(std::remove_if(g_Bullets.begin(), g_Bullets.end(),
                                   [](const Bullet &bullet)
                                   { return !bullet.active; }),
                    g_Bullets.end());
}

static void CheckBulletCollisions()
{
    std::lock_guard<std::mutex> playersLock(g_PlayersMutex);
    std::lock_guard<std::mutex> bulletsLock(g_BulletsMutex);

    for (auto &bullet : g_Bullets)
    {
        if (!bullet.active)
            continue;

        for (auto &player_pair : g_Players)
        {
            Player &player = player_pair.second;
            // Don't hit the shooter
            if (player.name == bullet.shooter_name)
                continue;

            // Check distance between bullet and player
            int dx = player.x - bullet.x;
            int dy = player.y - bullet.y;
            int distance = (int)sqrt(dx * dx + dy * dy);

            if (distance < 15)
            { // Hit radius
                bullet.active = false;
                player.score = (player.score - 1 > 0) ? (player.score - 1) : 0; // Don't go below 0
                std::cout << "[Bullet] Player " << player.name
                          << " hit by bullet from " << bullet.shooter_name
                          << " (score: " << player.score << ")\n";
                break; // Bullet can only hit one player
            }
        }
    }
}

// -------------------- Player Management --------------------
void AddPlayer(HQUIC stream, const std::string &name)
{
    std::lock_guard<std::mutex> lock(g_PlayersMutex);
    Player newPlayer;
    // Initial spawn point, can be randomized
    newPlayer.x = 50 + (rand() % 500);
    newPlayer.y = 50 + (rand() % 300);
    newPlayer.name = name.empty() ? "P" + std::to_string(g_NextPlayerId++) : name;
    newPlayer.stream = stream;
    g_Players[stream] = newPlayer;
    std::cout << "[Game] Player " << newPlayer.name << " joined at (" << newPlayer.x << "," << newPlayer.y << ")\n";
}

void RemovePlayer(HQUIC stream)
{
    std::lock_guard<std::mutex> lock(g_PlayersMutex);
    auto it = g_Players.find(stream);
    if (it != g_Players.end())
    {
        std::cout << "[Game] Player " << it->second.name << " left.\n";
        g_Players.erase(it);
    }
}

void HandlePlayerMove(HQUIC stream, int newX, int newY)
{
    std::lock_guard<std::mutex> lock(g_PlayersMutex);
    auto it = g_Players.find(stream);
    if (it != g_Players.end())
    {
        if (newX >= 0 && newY >= 0)
        {
            // Basic boundary checks for game area (e.g., 600x400)
            it->second.x = std::max(0, std::min(600, newX));
            it->second.y = std::max(0, std::min(400, newY));
        }
    }
}

void HandlePlayerShoot(const std::string &shooterName, int x, int y, int dx, int dy)
{
    // Normalize direction vector to ensure consistent speed regardless of angle
    double magnitude = std::sqrt(dx * dx + dy * dy);
    if (magnitude > 0)
    {
        CreateBullet(shooterName, x, y, (int)(dx / magnitude), (int)(dy / magnitude));
    }
    else
    {
        std::cerr << "[Gameplay] Shoot command with zero direction vector received from " << shooterName << std::endl;
    }
}

// -------------------- Game State Broadcast --------------------
void BroadcastGameState()
{
    // 1. Copy snapshot ra ngoài mutex
    std::vector<Player> playerSnapshot;
    std::vector<HQUIC> playerStreams;
    {
        std::lock_guard<std::mutex> lock(g_PlayersMutex);
        for (auto &kv : g_Players)
        {
            playerStreams.push_back(kv.first);
            playerSnapshot.push_back(kv.second);
        }
    }

    if (playerSnapshot.empty())
        return;

    // 2. Copy items snapshot
    std::vector<Item> itemsSnapshot;
    {
        std::lock_guard<std::mutex> lock(g_ItemsMutex);
        for (auto &item : g_Items)
        {
            if (item.active)
            {
                itemsSnapshot.push_back(item);
            }
        }
    }

    // 3. Copy bullets snapshot
    std::vector<Bullet> bulletsSnapshot;
    {
        std::lock_guard<std::mutex> lock(g_BulletsMutex);
        for (auto &bullet : g_Bullets)
        {
            if (bullet.active)
            {
                bulletsSnapshot.push_back(bullet);
            }
        }
    }

    // 4. Build JSON with players, items, and bullets
    json state_json = json::object();

    // Players array
    json players_json = json::array();
    for (auto &p : playerSnapshot)
    {
        players_json.push_back({{"name", p.name},
                                {"x", p.x},
                                {"y", p.y},
                                {"score", p.score}});
    }
    state_json["players"] = players_json;

    // Items array
    json items_json = json::array();
    for (auto &item : itemsSnapshot)
    {
        items_json.push_back({{"id", item.id},
                              {"x", item.x},
                              {"y", item.y}});
    }
    state_json["items"] = items_json;

    // Bullets array
    json bullets_json = json::array();
    for (auto &bullet : bulletsSnapshot)
    {
        bullets_json.push_back({{"id", bullet.id},
                                {"x", bullet.x},
                                {"y", bullet.y},
                                {"dx", bullet.dx},
                                {"dy", bullet.dy},
                                {"shooter", bullet.shooter_name}});
    }
    state_json["bullets"] = bullets_json;

    std::string msg = state_json.dump() + "\n";

    // 4. Send to each player
    for (size_t i = 0; i < playerStreams.size(); ++i)
    {
        HQUIC stream = playerStreams[i];
        // Allocate string on heap to be managed by the QUIC_STREAM_EVENT_SEND_COMPLETE callback
        auto *sendData = new std::string(msg);

        QUIC_BUFFER buf;
        buf.Buffer = (uint8_t *)sendData->data();
        buf.Length = (uint32_t)sendData->size();

        QUIC_STATUS status = MsQuic->StreamSend(
            stream,
            &buf,
            1,
            QUIC_SEND_FLAG_NONE, // Do not close stream after send
            sendData             // Pass sendData pointer as ClientContext
        );

        if (QUIC_FAILED(status))
        {
            std::cout << "[Game] Failed to send to " << playerSnapshot[i].name
                      << " (status: 0x" << std::hex << status << std::dec << ")\n";
            delete sendData; // Delete immediately if send fails
        }
    }
}

// -------------------- Broadcast Thread --------------------
void BroadcastLoop()
{
    srand((unsigned int)time(nullptr)); // Initialize random seed

    auto lastItemSpawn = std::chrono::steady_clock::now();
    const auto itemSpawnInterval = std::chrono::seconds(5); // Spawn item every 5 seconds

    while (g_ServerRunning.load())
    {
        auto now = std::chrono::steady_clock::now();

        // Check item collection
        CheckItemCollection();

        // Update bullets
        UpdateBullets();

        // Check bullet collisions
        CheckBulletCollisions();

        // Spawn new item if enough time has passed
        if (now - lastItemSpawn >= itemSpawnInterval)
        {
            SpawnItem();
            lastItemSpawn = now;
        }

        // Broadcast game state
        BroadcastGameState();
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // tick rate (20 ticks/second)
    }
}