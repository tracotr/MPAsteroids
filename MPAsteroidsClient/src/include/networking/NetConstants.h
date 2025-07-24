#pragma once

#include "../raylib/raymath.h"

#define MAX_PLAYERS 5

#define SCREEN_WIDTH 1280;
#define SCREEN_HEIGHT 720;

#define SERVER_PORT 25665;

enum NetworkCommands
{
    AcceptPlayer = 1,
    AddPlayer = 2,
    RemovePlayer = 3,
    UpdatePlayer = 4,
    UpdateInput = 5,
    AddAsteroid = 6,
    UpdateAsteroid = 7,
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

struct AsteroidPacket
{
    int Command;
    int Id;
    Vector3 Position;
    Matrix Rotation;
};
#pragma pack(pop)