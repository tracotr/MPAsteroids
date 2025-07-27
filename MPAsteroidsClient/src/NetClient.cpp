#include "include/NetClient.h"

#define ENET_IMPLEMENTATION
#include "include/networking/NetCommon.h"

#include <stdio.h>
#include <vector>

int localPlayerId = -1;

ENetAddress address = { 0 };
ENetHost* client = { 0 };
ENetPeer* server = { 0 };

// how long in seconds since the last time we sent an update
double lastInputSend = -100;

// how long to wait between updates (30 update ticks a second)
const double updateInterval = 1.0f / 30.0f;

double lastNow = 0;

struct RemotePlayers
{
    bool Active = false;

    Vector3 Position;
    Matrix Rotation;

    double LastUpdateTime;
};

static RemotePlayers players[MAX_PLAYERS] = { 0 };
 
AsteroidInfo Asteroids[MAX_ASTEROIDS];
int AsteroidAmount = 0;

void NetConnect(const char* serverAddress)
{
    if (enet_initialize() != 0)
    {
        return;
    }
    
    client = enet_host_create(NULL, 1, 2, 0 ,0);

    if (client == NULL) 
    {
        printf("client not found\n");
        exit(EXIT_FAILURE);
    }

    enet_address_set_host(&address, serverAddress);
    address.port = SERVER_PORT;

    server = enet_host_connect(client, &address, 2, 0);

    if (server == NULL) 
    {
        printf("server not found\n");
        exit(EXIT_FAILURE);
    }
    
}

// A new remote player was added to our local simulation
void HandleAddPlayer(PlayerPacket packet)
{
	// find out who the server is talking about
	int remotePlayer = packet.Id;

    // skip if out of bounds, or local player
	if (remotePlayer >= MAX_PLAYERS || remotePlayer == localPlayerId)
		return;

	// set them as active and update the location
	players[remotePlayer].Active = true;
	players[remotePlayer].Position = packet.Position;
	players[remotePlayer].Rotation = packet.Rotation;
	players[remotePlayer].LastUpdateTime = lastNow;
}

// A remote player has left the game and needs to be removed from the local simulation
void HandleRemovePlayer(PlayerPacket packet)
{
	// find out who the server is talking about
	int remotePlayer = packet.Id;

    // skip if out of bounds, or local player
	if (remotePlayer >= MAX_PLAYERS || remotePlayer == localPlayerId)
		return;

	// remove the player from the simulation. No other data is needed except the player id
	players[remotePlayer].Active = false;
}

// The server has a new position for a player in our local simulation
void HandleUpdatePlayer(PlayerPacket packet)
{
	// find out who the server is talking about
	int remotePlayer =  packet.Id;

    // skip if out of bounds, local player, or not active
	if (remotePlayer >= MAX_PLAYERS || remotePlayer == localPlayerId || !players[remotePlayer].Active)
		return;

	// update the last known position and movement
	players[remotePlayer].Position = packet.Position;
	players[remotePlayer].Rotation = packet.Rotation;
	players[remotePlayer].LastUpdateTime = lastNow;
}

int GetLocalPlayerId()
{
	return localPlayerId;
}

void UpdateLocalPlayer(Vector3 pos, Matrix rot)
{
    // if we are not accepted, we can't update
	if (localPlayerId < 0)
        return;

    // Update local player
    players[localPlayerId].Position = pos;
    players[localPlayerId].Rotation = rot;
}

bool GetPlayerSpatial(int id, Vector3* pos, Matrix* rot)
{
    // make sure the player is valid and active, or disregard our player id
    if (id < 0 || id >= MAX_PLAYERS || !players[id].Active || id == localPlayerId)
    {
        return false;
    }
    // copy the location of our networked friend
    *pos = players[id].Position;
    *rot = players[id].Rotation;
    // return true because they exist
    return true;
}


void HandleAddAsteroid(AsteroidPacket packet)
{
    //memcpy(Asteroids, packet.AllAsteroids, sizeof(packet.AllAsteroids));
    for (int i = 0; i < packet.AsteroidCount && i < MAX_ASTEROIDS; i++) {
        Asteroids[i] = packet.AllAsteroids[i];
    }
    AsteroidAmount = packet.AsteroidCount;
}

void HandleUpdateAsteroid(AsteroidPacket packet)
{
    for (int i = 0; i < packet.AsteroidCount && i < MAX_ASTEROIDS; i++) {
        Asteroids[i] = packet.AllAsteroids[i];
    }
    AsteroidAmount = packet.AsteroidCount;
}

bool GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot)
{
    if(id < 0 || id >= AsteroidAmount)
    {
        return false;
    }

    *pos = Asteroids[id].Position;
    *rot = Asteroids[id].Rotation;
    return true;
}

int GetMaxAsteroids()
{
    return AsteroidAmount;
}

void NetUpdate(double now, float delta)
{
    lastNow = now;

    // Skip if no server
    if (server == NULL)
        return;

    // if we're in a server send packet to server
    if (localPlayerId >= 0 && now - lastInputSend > updateInterval)
    {
        // construct a buffer and send it through a packet
        PlayerPacket buffer = { 0 };
        buffer.Command = UpdateInput;
        buffer.Position = players[localPlayerId].Position;
        buffer.Rotation = players[localPlayerId].Rotation;

        // create packet
        ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
        // send the data to the user
        enet_peer_send(server, 0, packet);

        lastInputSend = now;
    }

    ENetEvent event = {};
    if (enet_host_service(client, &event, 0) > 0)
    {  
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                printf("Connected to server\n");
                break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
				if (event.packet->dataLength < 1)
				{
					enet_packet_destroy(event.packet);
					break;
				}

                if (event.packet->dataLength == sizeof(PlayerPacket))
                {
                    // Recieve our packet sent from our players
                    PlayerPacket recieved;
                    memcpy(&recieved, event.packet->data, sizeof(PlayerPacket));

                    // If we have an id in the server, do what the server wants us to do
                    if (localPlayerId != -1)
                    {
                        switch (recieved.Command)
                        {
                            case AddPlayer:
                                HandleAddPlayer(recieved);
                                break;

                            case RemovePlayer:
                                HandleRemovePlayer(recieved);
                                break;

                            case UpdatePlayer:
                                HandleUpdatePlayer(recieved);
                                break;
                        }
                    }
                    // We do not have an ID in the server, so we need to read the accept command
                    else 
                    {
                        if (recieved.Command != AcceptPlayer) // Command SHOULD be accept player, but skip if it's not
                            return;

                        // Read id from command, check if in bounds, and prepare it to be in the game.
                        localPlayerId = recieved.Id;

                        if (localPlayerId < 0 || localPlayerId > MAX_PLAYERS)
                        {
                            localPlayerId = -1;
                            break;
                        }

                        lastInputSend = -updateInterval;
                        
                        players[localPlayerId].Active = true;
                        players[localPlayerId].Position = (Vector3){ 0.0f, 0.0f, 0.0f };
                    }
                }
                else if (event.packet->dataLength == sizeof(AsteroidPacket))
                {
                    // Recieve our packet sent from our players
                    AsteroidPacket recieved;
                    memcpy(&recieved, event.packet->data, sizeof(AsteroidPacket));

                    switch(recieved.Command)
                    {
                        case AddAsteroid:
                            HandleAddAsteroid(recieved);
                            break;
                        case UpdateAsteroid:
                            HandleUpdateAsteroid(recieved);
                            break;
                    }
                }

                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                // close our client
				if (client != NULL)
                    enet_host_destroy(client);

                client = NULL;
                server = NULL;

                // clean up enet
                enet_deinitialize();

                server = NULL;
                localPlayerId = -1;  
            }
            break;
        }
    }
}

