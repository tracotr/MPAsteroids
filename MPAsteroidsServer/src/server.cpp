#define ENET_IMPLEMENTATION
#include "include/net_common.h"

#include <stdio.h>
#include <vector>
#include <ctime>
#include <random>

struct PlayerInfo
{
    int Id;
    bool Active = false;
    bool ValidPosition = false;
    ENetPeer* Peer;
    Vector3 Position;
    Matrix Rotation;
};

PlayerInfo Players[MAX_PLAYERS] = { 0 };
 
AsteroidInfo Asteroids[MAX_ASTEROIDS];
int AsteroidAmount = 0;

double lastAsteroidUpdateTime = 0;
const double asteroidTickInterval = 1.0f / 30.0f; 

ENetAddress address = { 0 };
ENetHost* server = { 0 };

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> rvelocity(-2, 2);
std::uniform_real_distribution<> rdist(-25, 25);

double GetTime()
{
    return (double)clock() / CLOCKS_PER_SEC;
}

// sends a packet over the network to every active player, except the one specified (usually the sender)
void SendPacketToAllBut(ENetPacket* packet, int exceptPlayerId, int Channel)
{
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (!Players[i].Active || i == exceptPlayerId)
			continue;

        ENetPacket* cloned = enet_packet_create(packet->data, packet->dataLength, packet->flags);
		enet_peer_send(Players[i].Peer, Channel, cloned);
	}
}

// sends a packet over the network to every active player, except the one specified (usually the sender)
void SendPacketToOnly(ENetPacket* packet, int playerId, int Channel)
{
    ENetPacket* cloned = enet_packet_create(packet->data, packet->dataLength, packet->flags);
	enet_peer_send(Players[playerId].Peer, Channel, cloned);
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

AsteroidInfo CreateAsteroid()
{
    AsteroidInfo newAsteroid = { 0 };
    newAsteroid.Position = (Vector3){ rdist(gen), rdist(gen), rdist(gen) };
    newAsteroid.Velocity = (Vector3){ rvelocity(gen) , rvelocity(gen), rvelocity(gen) };
    newAsteroid.Rotation = MatrixIdentity();
    return newAsteroid;
}


void SpawnAsteroidsAroundPlayer(PlayerInfo* player, int amount)
{
    // spawn amount of asteroids 
    for(int i = 0; i < amount; i++)
    {   
        // skip if we're over max
        if(AsteroidAmount >= MAX_ASTEROIDS){
            break;
        }

        AsteroidInfo newAsteroid = CreateAsteroid();
        Asteroids[AsteroidAmount] = newAsteroid;
        AsteroidAmount += 1;
    }
}


Vector3 GetNearestPlayerPosition(Vector3 asteroidPos)
{
    float closestDistSq = MAX_SQR_V3;
    Vector3 closestPos = { 0 };

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!Players[i].Active)
            continue;

        float distSq = Vector3DistanceSqr(Players[i].Position, asteroidPos);
        if (distSq < closestDistSq)
        {
            closestDistSq = distSq;
            closestPos = Players[i].Position;
        }
    }

    return closestPos;
}

void WrapAsteroid(AsteroidInfo* asteroid, Vector3 center, float wrapDistance)
{
    Vector3& pos = asteroid->Position;

    if (pos.x > center.x + wrapDistance)
        pos.x = center.x - wrapDistance;
    else if (pos.x < center.x - wrapDistance)
        pos.x = center.x + wrapDistance;

    if (pos.y > center.y + wrapDistance)
        pos.y = center.y - wrapDistance;
    else if (pos.y < center.y - wrapDistance)
        pos.y = center.y + wrapDistance;

    if (pos.z > center.z + wrapDistance)
        pos.z = center.z - wrapDistance;
    else if (pos.z < center.z - wrapDistance)
        pos.z = center.z + wrapDistance;
}

void UpdateAsteroids(double delta)
{
    // Update position of asteroids
    for(int i = 0; i < AsteroidAmount; i++)
    {
        AsteroidInfo& asteroid = Asteroids[i];
        asteroid.Position = Vector3Add(asteroid.Position, Vector3Scale(asteroid.Velocity, delta));
        Vector3 closestPlayerPos = GetNearestPlayerPosition(asteroid.Position);
        WrapAsteroid(&asteroid, closestPlayerPos, MAX_ASTEROID_DIST); 
    }

    // send position to players
    AsteroidInfoPacket buffer = { 0 };
    buffer.Command = UpdateAsteroid;
    memcpy(buffer.AllAsteroids, Asteroids, sizeof(Asteroids));
    buffer.AsteroidCount = AsteroidAmount;

    // create packet
    ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_UNSEQUENCED);
    // send to all connected clients
    SendPacketToAllBut(packet, -1, 1);
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
        double now = GetTime();
        double delta = now - lastAsteroidUpdateTime;

        // Only update asteroid movement if enough time has passed
        if (delta >= asteroidTickInterval)
        {
            UpdateAsteroids(delta);
            lastAsteroidUpdateTime = now;
        }

        // Check for events (updates from clients)
        ENetEvent event = {};
        if (enet_host_service(server, &event, 1) > 0)
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
                    Players[playerId].Id = playerId;

                    // valid position stops an update being sent until theyre in a good spot
                    Players[playerId].ValidPosition = false;
                    Players[playerId].Peer = event.peer;

                    // construct a buffer and send it through a packet
                    PlayerPacket acceptBuffer = { 0 };
                    acceptBuffer.Command = AcceptPlayer;
                    acceptBuffer.Id = playerId;

                    // create packet
					ENetPacket* accept_player_packet = enet_packet_create(&acceptBuffer, sizeof(acceptBuffer), ENET_PACKET_FLAG_RELIABLE);
					// send the data to the user
					SendPacketToOnly(accept_player_packet, playerId, 0);

                    // Tell new client of other players
                    for (int i = 0; i < MAX_PLAYERS; i++){
                        // Skip over new player
                        if(i == playerId || !Players[i].ValidPosition)
                            continue;
                        
                        // Construct a new buffer
                        PlayerPacket otherBuffer = { 0 };
                        otherBuffer.Command = AddPlayer;
                        otherBuffer.Id = i;
                        otherBuffer.Position = Players[i].Position;
                        otherBuffer.Rotation = Players[i].Rotation;

                        // create a new packet and send it to new player
                        ENetPacket* other_player_packet  = enet_packet_create(&otherBuffer, sizeof(otherBuffer), ENET_PACKET_FLAG_RELIABLE);
                        SendPacketToOnly(other_player_packet, playerId, 0);
                    }

                    // send packet to all other players that new player has joined
                    PlayerPacket allOtherBuffer = { 0 };
                    allOtherBuffer.Command = AddPlayer;
                    allOtherBuffer.Id = playerId;
                    allOtherBuffer.Position = Players[playerId].Position;
                    allOtherBuffer.Rotation = Players[playerId].Rotation;
                    ENetPacket* other_players_packet = enet_packet_create(&allOtherBuffer, sizeof(allOtherBuffer), ENET_PACKET_FLAG_RELIABLE);
                    SendPacketToAllBut(other_players_packet, playerId, 0);

                    // Spawn new asteroids around new player
                    SpawnAsteroidsAroundPlayer(&Players[playerId], 70);

                    // send new player info about all asteroids
                    AsteroidInfoPacket asteroid_buffer = { 0 };
                    asteroid_buffer.Command = AddAsteroid;
                    memcpy(asteroid_buffer.AllAsteroids, Asteroids, sizeof(Asteroids));
                    asteroid_buffer.AsteroidCount = AsteroidAmount;
                    ENetPacket* asteroid_packet = enet_packet_create(&asteroid_buffer, sizeof(asteroid_buffer), ENET_PACKET_FLAG_RELIABLE);
                    SendPacketToOnly(asteroid_packet, playerId, 1);

                    break;
                }
                
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    if(event.packet->dataLength == sizeof(PlayerPacket))
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
                            ENetPacket* packet = enet_packet_create(&updatePlayerPacket, sizeof(updatePlayerPacket), 0);
                            // send data to all users besides the usser
                            SendPacketToAllBut(packet, playerId, 0);
                        }
                    }
                    else if(event.packet->dataLength == sizeof(AsteroidDestroyPacket))
                    {
                        AsteroidDestroyPacket recieved;
                        memcpy(&recieved, event.packet->data, sizeof(AsteroidDestroyPacket));
                        printf("%i, %i", recieved.AsteroidID, recieved.PlayerID);
                        Asteroids[recieved.AsteroidID] = CreateAsteroid();
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
                    ENetPacket* remove_player_packet = enet_packet_create(&removePlayerPacket, sizeof(removePlayerPacket), ENET_PACKET_FLAG_RELIABLE);
                    // send data to all users
                    SendPacketToAllBut(remove_player_packet, -1, 0);

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