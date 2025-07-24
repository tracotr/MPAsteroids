#define ENET_IMPLEMENTATION
#include "include/net_common.h"

#include <stdio.h>
#include <vector>

struct PlayerInfo
{
    bool Active = false;
    bool ValidPosition = false;
    ENetPeer* Peer;
    Vector3 Position;
    Matrix Rotation;
};

PlayerInfo Players[MAX_PLAYERS] = { 0 };

struct AsteroidInfo
{
    int Id;
    bool Active = false;
    Vector3 Position;
    Vector3 Velocity;
    Matrix Rotation;
};
 
std::vector<AsteroidInfo> Asteroids;

ENetAddress address = { 0 };
ENetHost* server = { 0 };

// sends a packet over the network to every active player, except the one specified (usually the sender)
void SendToAllBut(ENetPacket* packet, int exceptPlayerId)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (!Players[i].Active || i == exceptPlayerId)
			continue;

		enet_peer_send(Players[i].Peer, 0, packet);
	}
}

// finds player id of peer specified
int GetPlayerId(ENetPeer* peer)
{
	// find the slot that matches the pointer
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (Players[i].Active && Players[i].Peer == peer)
			return i;
	}
	return -1;
}

void SpawnAsteroidsAroundPlayer(Vector3 playerPosition)
{
    // spawn asteroids for a test
    for(int i = 0; i < 20; i++)
    {
        AsteroidInfo newAsteroid;
        newAsteroid.Id = i;
        newAsteroid.Position = (Vector3){ };
        newAsteroid.Velocity = (Vector3){ 0.0f , 0.0f, 0.0f };
        newAsteroid.Rotation = MatrixIdentity();
        Asteroids.push_back(newAsteroid);
    }
}

int main()
{
    if (enet_initialize () != 0)
    {
        printf("An error occurred while initializing ENet.\n");
        return 1;
    }

	address.host = ENET_HOST_ANY;
	address.port = SERVER_PORT;

    // create the server host
	server = enet_host_create(&address, MAX_PLAYERS, 2, 0, 0);
    
    if (server == NULL) 
    {
        printf("An error occurred while trying to create an ENet server host.\n");
        return 1;
    }
    
    // server run successful, run server loop
    printf("Server running successfully\n");

    bool run = true;
	while (run)
	{
        // Update and send position of asteroids to players
        for (auto& asteroid : Asteroids)
        {   
            // update position of asteroid
            asteroid.Position = Vector3Add(asteroid.Position, asteroid.Velocity);

            // send position to players
            AsteroidPacket buffer = { 0 };
            buffer.Command = UpdateAsteroid;
            buffer.Id = asteroid.Id;
            buffer.Position = asteroid.Position;
            buffer.Rotation = asteroid.Rotation;

            // create packet
            ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
            // send to all connected clients
            SendToAllBut(packet, -1);
        }

        // Check for events (updates from clients)
        ENetEvent event = {};
        if (enet_host_service(server, &event, 0) > 0)
		{
            switch(event.type)
            {   
                // Someone new joined
                case ENET_EVENT_TYPE_CONNECT:
                {
                    printf("A new client connected\n");

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
						enet_peer_disconnect(event.peer, 0);
						break;
					}

                    // Set new player to active and associate its peer
                    Players[playerId].Active = true;

                    // valid position stops an update being sent until theyre in a good spot
                    Players[playerId].ValidPosition = false;
                    Players[playerId].Peer = event.peer;

                    // construct a buffer and send it through a packet
                    PlayerPacket buffer = { 0 };
                    buffer.Command = AcceptPlayer;
                    buffer.Id = playerId;

                    // create packet
					ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
					// send the data to the user
					enet_peer_send(event.peer, 0, packet);

                    // Tell new client of other players
                    for (int i = 0; i < MAX_PLAYERS; i++){
                        // Skip over new player
                        if(i == playerId || !Players[i].ValidPosition)
                            continue;
                        
                        // Construct a new buffer
                        PlayerPacket newBuffer = { 0 };
                        newBuffer.Command = AddPlayer;
                        newBuffer.Id = i;
                        newBuffer.Position = Players[i].Position;
                        newBuffer.Rotation = Players[i].Rotation;

                        // create a new packet and send it to new player
                        packet = enet_packet_create(&newBuffer, sizeof(newBuffer), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 0, packet);
                    }

                    // send packet to all other players that new player has joined
                    PlayerPacket otherBuffer = { 0 };
                    otherBuffer.Command = AddPlayer;
                    otherBuffer.Id = playerId;
                    otherBuffer.Position = Players[playerId].Position;
                    otherBuffer.Rotation = Players[playerId].Rotation;
                    packet = enet_packet_create(&otherBuffer, sizeof(otherBuffer), ENET_PACKET_FLAG_RELIABLE);
                    SendToAllBut(packet, playerId);

                    // send new player info about all asteroids
                    for (AsteroidInfo asteroid : Asteroids)
                    {
                        AsteroidPacket buffer = { 0 };
                        buffer.Command = AddAsteroid;
                        buffer.Id = asteroid.Id;
                        buffer.Position = asteroid.Position;
                        buffer.Rotation = asteroid.Rotation;

                        packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
                        enet_peer_send(event.peer, 0, packet);
                    }

                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    int playerId = GetPlayerId(event.peer);

                    // boot event if not in our list of active players
                    if (playerId == -1)
					{
						enet_peer_disconnect(event.peer, 0);
						break;
					}
                    
                    PlayerPacket recieved;
                    memcpy(&recieved, event.packet->data, sizeof(PlayerPacket));

                    NetworkCommands command = (NetworkCommands)recieved.Command;

                    if (command == UpdateInput)
                    {
                        Players[playerId].Position = recieved.Position;
                        Players[playerId].Rotation = recieved.Rotation;
                    
                        PlayerPacket updatePlayerPacket;
                        updatePlayerPacket.Command = UpdatePlayer;
                    
                        if (!Players[playerId].ValidPosition)
                        {
                            updatePlayerPacket.Command = UpdatePlayer;
                        }
                    
                        // if theyre good, can send as regular updates
                        Players[playerId].ValidPosition = true;
                        
                        // update our new packet with correct info
                        updatePlayerPacket.Id = playerId;
                        updatePlayerPacket.Position = Players[playerId].Position;
                        updatePlayerPacket.Rotation = Players[playerId].Rotation;
                    
                        // create packet
                        ENetPacket* packet = enet_packet_create(&updatePlayerPacket, sizeof(updatePlayerPacket), ENET_PACKET_FLAG_RELIABLE);
                        // send data to all users besides the usser
                        SendToAllBut(packet, playerId);
                    }

                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    printf("player disconnected\n");

                    int playerId = GetPlayerId(event.peer);
                    if (playerId == -1)
                    {
                        break;
                    }

                    Players[playerId].Active = false;
                    Players[playerId].Peer = NULL;
                    Players[playerId].Position = {0};
                    Players[playerId].Rotation = MatrixIdentity();

                    PlayerPacket removePlayerPacket;
                    removePlayerPacket.Command = RemovePlayer;
                    removePlayerPacket.Id = playerId;

                    // create packet
                    ENetPacket* packet = enet_packet_create(&removePlayerPacket, sizeof(removePlayerPacket), ENET_PACKET_FLAG_RELIABLE);
                    // send data to all users
                    SendToAllBut(packet, -1);

                    break;
                }
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }
    
    enet_host_destroy(server);
    enet_deinitialize();

    return 0;
}