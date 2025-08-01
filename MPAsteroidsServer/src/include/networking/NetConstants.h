#pragma once

#include "../raylib/raymath.h"

#define MAX_PLAYERS 5
#define MAX_ASTEROIDS 48
#define MAX_ASTEROID_DIST 27.0f
#define MAX_SQR_V3 3.402823466e+38F

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define SERVER_PORT 25665

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
};

#pragma pack(push, 1)
struct PlayerPacket
{
    int Command;
    int Id;
    bool Active;
    Vector3 Position;
    Matrix Rotation;
};

struct AsteroidInfo
{
    bool Active = false;
    Vector3 Position = { 0 };
    Vector3 Velocity = { 0 };
    Matrix Rotation = MatrixIdentity();
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
    int Id;
};
#pragma pack(pop)