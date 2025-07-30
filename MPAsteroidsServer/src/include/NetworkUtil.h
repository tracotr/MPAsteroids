#pragma once
#include "net_common.h"
#include <stdio.h>
#include <string>

namespace NetworkUtil
{
    void SendPacketToAllBut(ENetPacket* packet, int exceptPlayerId, int channel);
    void SendPacketToOnly(ENetPacket* packet, int playerId, int channel);
    void Log(const std::string& msg);
};