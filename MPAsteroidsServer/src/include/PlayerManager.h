#pragma once
#include "net_common.h"
#include "PlayerInfo.h"

class PlayerManager
{   
public:
    static PlayerInfo Players[MAX_PLAYERS];

    void UpdateInput(PlayerPacket* received, int playerId);
    int InitializeNewPlayer(ENetEvent* event);
    void DisconnectPlayer(int playerId);
    int GetPlayerId(ENetPeer* peer);
};