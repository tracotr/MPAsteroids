#include "include/NetworkUtil.h"
#include "include/PlayerManager.h"
#include "include/PlayerInfo.h"

namespace NetworkUtil
{
    // sends a packet over the network to every active player, except the one specified (usually the sender)
    void SendPacketToAllBut(ENetPacket* packet, int exceptPlayerId, int channel)
    {
        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            if (!PlayerManager::Players[i].Active || i == exceptPlayerId)
                continue;

            ENetPacket* cloned = enet_packet_create(packet->data, packet->dataLength, packet->flags);
            enet_peer_send(PlayerManager::Players[i].Peer, channel, cloned);
        }
    }

    // sends a packet over the network to every active player, except the one specified (usually the sender)
    void SendPacketToOnly(ENetPacket* packet, int playerId, int channel)
    {
        ENetPacket* cloned = enet_packet_create(packet->data, packet->dataLength, packet->flags);
        enet_peer_send(PlayerManager::Players[playerId].Peer, channel, cloned);
    }

    // logging function, cant really print anything besides strings right now tho
    void Log(const std::string& msg)
    {   
        printf("[SERVER]: %s\n", msg.c_str());
    }
}