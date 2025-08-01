#pragma once
#include "networking/NetCommon.h"
#include "PlayerInfo.h"
#include "NetworkUtil.h"

class PlayerManager
{   
private:
    static PlayerInfo Players[MAX_PLAYERS];

public:
    void UpdateInput(PlayerPacket* received, int playerId);
    int InitializeNewPlayer(ENetEvent* event);
    void DisconnectPlayer(int playerId);
    int GetPlayerId(ENetPeer* peer);

    PlayerInfo (&GetPlayers())[MAX_PLAYERS] { return Players; };
};