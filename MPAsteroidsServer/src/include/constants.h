#pragma once

#include "raylib/raymath.h"

#define MAX_PLAYERS 5
#define MAX_ASTEROIDS 64
#define MAX_ASTEROID_DIST 30.0f
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
    AddAsteroid = 6,
    UpdateAsteroid = 7,
    DestroyAsteroid = 8,
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
    Vector3 Position;
    Vector3 Velocity;
    Matrix Rotation;
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
#pragma pack(pop)