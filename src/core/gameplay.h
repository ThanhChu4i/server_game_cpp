#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include "msquic.h" // For HQUIC

struct Player
{
    int x, y;
    std::string name;
    HQUIC stream;
    int score = 0;
};

struct Item
{
    int x, y;
    int id;
    bool active = true;
};

struct Bullet
{
    int x, y;
    int dx, dy; // direction vector
    int id;
    std::string shooter_name;
    bool active = true;
    int speed = 5; // pixels per update
};

// Global game state variables
extern std::map<HQUIC, Player> g_Players;
extern std::mutex g_PlayersMutex;

extern std::vector<Item> g_Items;
extern std::mutex g_ItemsMutex;
extern int g_NextItemId;

extern std::vector<Bullet> g_Bullets;
extern std::mutex g_BulletsMutex;
extern int g_NextBulletId;

extern int g_NextPlayerId;

// Game logic functions
void AddPlayer(HQUIC stream, const std::string &name);
void RemovePlayer(HQUIC stream);
void BroadcastGameState();
void HandlePlayerMove(HQUIC stream, int newX, int newY);
void HandlePlayerShoot(const std::string &shooterName, int x, int y, int dx, int dy);

// Game loop function
void BroadcastLoop();