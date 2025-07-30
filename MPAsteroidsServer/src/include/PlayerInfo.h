#ifndef PLAYER_INFO_H
#define PLAYER_INFO_H

#include "net_common.h"

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