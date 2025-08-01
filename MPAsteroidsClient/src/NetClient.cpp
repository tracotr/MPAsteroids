#include "include/NetClient.h"
#include "include/networking/NetCommon.h"

ENetAddress Address = { 0 };
ENetHost* Client = { 0 };
ENetPeer* Server = { 0 };

void NetClient::NetConnect(const char* serverAddress)
{
    if (enet_initialize() != 0)
    {
        return;
    }
    
    Client = enet_host_create(NULL, 1, 2, 0 ,0);

    if (Client == NULL) 
    {
        printf("client not found\n");
        exit(EXIT_FAILURE);
    }

    enet_address_set_host(&Address, serverAddress);
    Address.port = SERVER_PORT;

    Server = enet_host_connect(Client, &Address, 2, 0);

    if (Server == NULL) 
    {
        printf("server not found\n");
        exit(EXIT_FAILURE);
    }
}

// A new remote player was added to our local simulation
void NetClient::HandleAddPlayer(PlayerPacket packet)
{
	// find out who the server is talking about
	int remotePlayer = packet.Id;

    // skip if out of bounds, or local player
	if (remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId)
		return;

	// set them as active and update the location
	Players[remotePlayer].Active = true;
	Players[remotePlayer].Position = packet.Position;
	Players[remotePlayer].Rotation = packet.Rotation;
	Players[remotePlayer].LastUpdateTime = LastNow;
}

// A remote player has left the game and needs to be removed from the local simulation
void NetClient::HandleRemovePlayer(PlayerPacket packet)
{
	// find out who the server is talking about
	int remotePlayer = packet.Id;

    // skip if out of bounds, or local player
	if (remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId)
		return;

	// remove the player from the simulation. No other data is needed except the player id
	Players[remotePlayer].Active = false;
}

// The server has a new position for a player in our local simulation
void NetClient::HandleUpdatePlayer(PlayerPacket packet)
{
	// find out who the server is talking about
	int remotePlayer =  packet.Id;

    // skip if out of bounds, local player, or not active
	if (remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId || !Players[remotePlayer].Active)
		return;

	// update the last known position and movement
	Players[remotePlayer].Position = packet.Position;
	Players[remotePlayer].Rotation = packet.Rotation;
	Players[remotePlayer].LastUpdateTime = LastNow;
}

void NetClient::UpdateLocalPlayer(Vector3 pos, Matrix rot)
{
    // if we are not accepted, we can't update
	if (LocalPlayerId < 0)
        return;

    // Update local player
    Players[LocalPlayerId].Position = pos;
    Players[LocalPlayerId].Rotation = rot;
}

bool NetClient::GetPlayerSpatial(int id, Vector3* pos, Matrix* rot)
{
    // make sure the player is valid and active, or disregard our player id
    if (id < 0 || id >= MAX_PLAYERS || !Players[id].Active || id == LocalPlayerId)
    {
        return false;
    }

    // copy the location of our networked friend
    *pos = Players[id].Position;
    *rot = Players[id].Rotation;

    // return true because they exist
    return true;
}

void NetClient::HandlePlayerCollision()
{
    ScoreboardPacket scoreboardBuffer;
    scoreboardBuffer.Command = NetworkCommands::ResetScoreboardId;
    scoreboardBuffer.Id = LocalPlayerId;

    // create packet
    ENetPacket* packet = enet_packet_create(&scoreboardBuffer, sizeof(scoreboardBuffer), ENET_PACKET_FLAG_RELIABLE);
    
    // send the data to the user
    enet_peer_send(Server, 0, packet);
}

void NetClient::HandleUpdateAsteroid(AsteroidInfoPacket packet)
{
    for (int i = 0; i < packet.AsteroidCount && i < MAX_ASTEROIDS; i++) {
        Asteroids[i] = packet.AllAsteroids[i];
    }
    AsteroidAmount = packet.AsteroidCount;
}

void NetClient::HandleDestroyAsteroid(int playerIdx, int asteroidIdx)
{
    // create buffer
    AsteroidDestroyPacket buffer = { 0 };
    buffer.Command = NetworkCommands::DestroyAsteroid;
    buffer.PlayerID = playerIdx;
    buffer.AsteroidID = asteroidIdx;

    // create packet
    ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
    
    // send the data to the user
    enet_peer_send(Server, 0, packet);
}

bool NetClient::GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot)
{   
    // skip if out of bounds
    if(id < 0 || id >= AsteroidAmount)
    {
        return false;
    }

    *pos = Asteroids[id].Position;
    *rot = Asteroids[id].Rotation;
    return true;
}

void NetClient::HandleUpdateScoreboard(ScoreboardPacket packet)
{
    // copy argument into our scoreboard
    memcpy(Scoreboard, packet.Scoreboard, sizeof(packet.Scoreboard));
}


void NetClient::NetUpdate(double now, float delta)
{
    LastNow = now;

    // Skip if no server
    if (Server == NULL)
        return;

    // if we're in a server send packet to server
    if(LocalPlayerId >= 0 && now - LastInputSend > UPDATE_INTERVAL)
    {
        // construct a buffer and send it through a packet
        PlayerPacket buffer = { 0 };
        buffer.Command = UpdateInput;
        buffer.Position = Players[LocalPlayerId].Position;
        buffer.Rotation = Players[LocalPlayerId].Rotation;

        // create packet
        ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
        // send the data to the server
        enet_peer_send(Server, 0, packet);

        LastInputSend = now;
    }

    ENetEvent event = {};
    if(enet_host_service(Client, &event, 0) > 0)
    {  
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
                printf("Connected to server\n");
                break;

            case ENET_EVENT_TYPE_RECEIVE:
            {
				if(event.packet->dataLength < 1)
				{
					enet_packet_destroy(event.packet);
					break;
				}

                if(event.packet->dataLength == sizeof(PlayerPacket))
                {   
                    // Recieve our packet sent from our players
                    PlayerPacket recieved;
                    memcpy(&recieved, event.packet->data, sizeof(PlayerPacket));

                    // If we have an id in the server, do what the server wants us to do
                    if (LocalPlayerId != -1)
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
                        if (recieved.Command != NetworkCommands::AcceptPlayer) // Command SHOULD be accept player, but skip if it's not
                            return;

                        // Read id from command, check if in bounds, and prepare it to be in the game.
                        LocalPlayerId = recieved.Id;

                        if (LocalPlayerId < 0 || LocalPlayerId > MAX_PLAYERS)
                        {
                            LocalPlayerId = -1;
                            break;
                        }

                        LastInputSend = -UPDATE_INTERVAL;
                        
                        Players[LocalPlayerId].Active = true;
                        Players[LocalPlayerId].Position = (Vector3){ 0.0f, 0.0f, 0.0f };
                    }
                }
                else if(event.packet->dataLength == sizeof(AsteroidInfoPacket))
                {
                    // Recieve our packet sent from our players
                    AsteroidInfoPacket recieved;
                    memcpy(&recieved, event.packet->data, sizeof(AsteroidInfoPacket));

                    if(recieved.Command == NetworkCommands::UpdateAsteroid)
                    {
                        HandleUpdateAsteroid(recieved);
                    }
                }
                else if(event.packet->dataLength == sizeof(ScoreboardPacket))
                {
                    ScoreboardPacket recieved;
                    memcpy(&recieved, event.packet->data, sizeof(ScoreboardPacket));
                    if(recieved.Command == NetworkCommands::UpdateScoreboard)
                    {
                        HandleUpdateScoreboard(recieved);
                    }
                }

                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
            case ENET_EVENT_TYPE_DISCONNECT:
            {
                // close our client
				if (Client != NULL)
                    enet_host_destroy(Client);

                Client = NULL;
                Server = NULL;

                // clean up enet
                enet_deinitialize();

                Server = NULL;
                LocalPlayerId = -1;
                break;
            }
            case ENET_EVENT_TYPE_NONE:
            break;
        }
    }
}

