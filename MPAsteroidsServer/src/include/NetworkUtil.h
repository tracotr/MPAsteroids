#pragma once
#include "networking/NetCommon.h"
#include "PlayerInfo.h"
#include <stdio.h>
#include <string>

namespace NetworkUtil
{
    void SendPacketToAllBut(ENetPacket* packet, PlayerInfo (&players)[MAX_PLAYERS], int exceptPlayerId, int channel);
    void SendPacketToOnly(ENetPacket* packet, PlayerInfo (&players)[MAX_PLAYERS], int playerId, int channel);
    void Log(const std::string& msg);
};