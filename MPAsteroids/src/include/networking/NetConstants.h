#pragma once

#include "../raylib/raymath.h"

#define MAX_PLAYERS 64
#define MAX_ASTEROIDS 256
#define MAX_ASTEROID_DIST 22.0f
#define MAX_SQR_V3 3.402823466e+38F
#define MAX_PLAYER_NAME_LENGTH 16
#define MIN_ASTEROID_SCALE 0.5f

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

// Port the server binds. Behind a reverse proxy this stays private to the host.
#ifndef SERVER_PORT
#define SERVER_PORT 25665
#endif

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif

// Port the browser dials, which behind a proxy is the public HTTPS port rather
// than SERVER_PORT: the proxy accepts 443 and forwards to the server's local port.
#ifndef SERVER_PUBLIC_PORT
#define SERVER_PUBLIC_PORT SERVER_PORT
#endif

// URL path the proxy routes to the server, letting the site and game share a domain.
#ifndef SERVER_PATH
#define SERVER_PATH "/"
#endif

// Players past this are still scored, just not all listed at once.
#define LEADERBOARD_VISIBLE_ROWS 8

enum NetworkCommands
{
    AcceptPlayer = 1,
    AddPlayer = 2,
    RemovePlayer = 3,
    UpdatePlayer = 4,
    UpdateInput = 5,
    UpdateAsteroid = 6,
    DestroyAsteroid = 7,
    UpdateScoreboard = 8,
    ResetScoreboardId = 9,
    FireProjectile = 10,
};

#pragma pack(push, 1)

struct PlayerPacket
{
    int Command;
    int Id;
    bool Active;
    char Name[MAX_PLAYER_NAME_LENGTH];
    Vector3 Position;
    Matrix Rotation;
};

struct AsteroidInfo
{
    bool Active = false;
    Vector3 Position;
    Vector3 Velocity;
    Matrix Rotation;
    float Scale = 1.0f;
};

struct AsteroidInfoPacket
{   
    int Command;
    AsteroidInfo AllAsteroids[MAX_ASTEROIDS];
    int AsteroidCount;
};

struct AsteroidDestroyPacket
{
    int Command;
    int PlayerID;
    int AsteroidID;
};

struct ScoreboardPacket
{
    int Command;
    int Scoreboard[MAX_PLAYERS];
    char Names[MAX_PLAYERS][MAX_PLAYER_NAME_LENGTH];
    int Id;
};

struct ProjectilePacket
{
    int Command;
    int PlayerID;
    Vector3 Position;
    Vector3 Velocity;
};
#pragma pack(pop)