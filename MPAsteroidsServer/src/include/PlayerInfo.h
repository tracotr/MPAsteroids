#ifndef PLAYER_INFO_H
#define PLAYER_INFO_H

#include "networking/NetCommon.h"
#include "raylib/raymath.h"

struct PlayerInfo
{
    int Id;
    bool Active = false;
    bool ValidPosition = false;
    ENetPeer* Peer = { 0 };
    Vector3 Position = { 0 };
    Matrix Rotation = { 0 };
    int score = 0;
};

#endif