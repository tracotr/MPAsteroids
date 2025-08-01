#include "include/PlayerManager.h"


PlayerInfo PlayerManager::Players[MAX_PLAYERS]; 

// sends any input updates from players to other players
void PlayerManager::UpdateInput(PlayerPacket* received, int playerId)
{
    PlayerInfo& player = Players[playerId];

    player.Position = received->Position;
    player.Rotation = received->Rotation;

    PlayerPacket updatePlayerPacket;
    updatePlayerPacket.Command = NetworkCommands::UpdatePlayer;

    if (!player.ValidPosition)
    {
        updatePlayerPacket.Command = NetworkCommands::AddPlayer;
    }

    // if theyre good, can send as regular updates
    player.ValidPosition = true;
    
    // update our new packet with correct info
    updatePlayerPacket.Id = playerId;
    updatePlayerPacket.Position = player.Position;
    updatePlayerPacket.Rotation = player.Rotation;

    // create packet
    ENetPacket* packet = enet_packet_create(&updatePlayerPacket, sizeof(updatePlayerPacket), 0);
    // send data to all users besides the user
    NetworkUtil::SendPacketToAllBut(packet, Players, playerId, 2);
}

// makes a new player and returns their id
int PlayerManager::InitializeNewPlayer(ENetEvent* event)
{
    NetworkUtil::Log("New client connected");

    int playerId = 0;

    // Search for non-active player slot, loop until empty slot
    for(; playerId < MAX_PLAYERS; playerId++)
    {
        if(!Players[playerId].Active)
            break;
    }

    // disconnect if full
    if (playerId == MAX_PLAYERS)
    {
        enet_peer_disconnect(event->peer, 0);
        return -1;
    }

    // Set new player to active and associate its peer
    Players[playerId].Active = true;
    Players[playerId].Id = playerId;

    // valid position stops an update being sent until theyre in a good spot
    Players[playerId].ValidPosition = false;
    Players[playerId].Peer = event->peer;

    // construct a buffer and send it through a packet
    PlayerPacket acceptBuffer = { 0 };
    acceptBuffer.Command = NetworkCommands::AcceptPlayer;
    acceptBuffer.Id = playerId;

    // create packet
    ENetPacket* accept_player_packet = enet_packet_create(&acceptBuffer, sizeof(acceptBuffer), ENET_PACKET_FLAG_RELIABLE);
    // send the data to the user
    NetworkUtil::SendPacketToOnly(accept_player_packet, Players, playerId, 0);

    // Tell new client of other players
    for (int i = 0; i < MAX_PLAYERS; i++){
        // Skip over new player
        if(i == playerId || !Players[i].ValidPosition)
            continue;
        
        // Construct a new buffer
        PlayerPacket otherBuffer = { 0 };
        otherBuffer.Command = NetworkCommands::AddPlayer;
        otherBuffer.Id = i;
        otherBuffer.Position = Players[i].Position;
        otherBuffer.Rotation = Players[i].Rotation;

        // create a new packet and send it to new player
        ENetPacket* other_player_packet  = enet_packet_create(&otherBuffer, sizeof(otherBuffer), ENET_PACKET_FLAG_RELIABLE);
        NetworkUtil::SendPacketToOnly(other_player_packet, Players, playerId, 0);
    }

    // send packet to all other players that new player has joined
    PlayerPacket allOtherBuffer = { 0 };
    allOtherBuffer.Command = NetworkCommands::AddPlayer;
    allOtherBuffer.Id = playerId;
    allOtherBuffer.Position = Players[playerId].Position;
    allOtherBuffer.Rotation = Players[playerId].Rotation;
    ENetPacket* other_players_packet = enet_packet_create(&allOtherBuffer, sizeof(allOtherBuffer), ENET_PACKET_FLAG_RELIABLE);
    NetworkUtil::SendPacketToAllBut(other_players_packet, Players, playerId, 0);

    return playerId;
}

// removes a player from the array and tells other users
void PlayerManager::DisconnectPlayer(int playerId)
{
    Players[playerId].Active = false;
    Players[playerId].Peer = NULL;
    Players[playerId].Position = { 0 };
    Players[playerId].Rotation = MatrixIdentity();

    PlayerPacket removePlayerPacket;
    removePlayerPacket.Command = NetworkCommands::RemovePlayer;
    removePlayerPacket.Id = playerId;
    // create packet
    ENetPacket* remove_player_packet = enet_packet_create(&removePlayerPacket, sizeof(removePlayerPacket), ENET_PACKET_FLAG_RELIABLE);
    // send data to all users
    NetworkUtil::SendPacketToAllBut(remove_player_packet, Players, -1, 0);
}

// finds player id of peer specified
int PlayerManager::GetPlayerId(ENetPeer* peer)
{
	// find the slot that matches the pointer
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (Players[i].Active && Players[i].Peer == peer)
			return i;
	}
	return -1;
}

