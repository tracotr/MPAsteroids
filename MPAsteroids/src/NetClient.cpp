#include "include/NetClient.h"
#include "include/networking/NetCommon.h"

#include <cstring>

ENetAddress Address = { 0 };
ENetHost* Client = { 0 };
ENetPeer* Server = { 0 };
static bool g_enetInitialized = false;

bool EnsureENetReady()
{
    if (g_enetInitialized)
        return true;

    if (enet_initialize() != 0)
    {
        printf("Failed to initialize ENet\n");
        return false;
    }

    g_enetInitialized = true;
    return true;
}

namespace
{
    void CopySafeName(char* destination, size_t destinationSize, const char* source)
    {
        if (destination == nullptr || destinationSize == 0)
            return;

        std::memset(destination, 0, destinationSize);
        if (source == nullptr)
            return;

        std::strncpy(destination, source, destinationSize - 1);
        destination[destinationSize - 1] = '\0';
    }
}

bool NetClient::NetConnect(const char* serverAddress, const char* playerName)
{
    // Tear down any previous attempt first, so retrying with a different
    // address (e.g. after a typo or a timed-out connection) doesn't leak
    // the old client host or leave stale peer state behind.
    if (Client != NULL)
    {
        enet_host_destroy(Client);
        Client = NULL;
        Server = NULL;
    }

    LocalPlayerId = -1;
    CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), playerName);
    if (LocalPlayerName[0] == '\0')
        CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), "Player");

    if (!EnsureENetReady())
    {
        return false;
    }
    
    // create the server host
    // channel 0: reliable packets
    // channel 1: asteroid updates
    // channel 2: player updates
    Client = enet_host_create(NULL, 1, 3, 0 ,0);

    if(Client == NULL) 
    {
        return false;
    }

    enet_address_set_host(&Address, serverAddress);
    Address.port = SERVER_PORT;

    Server = enet_host_connect(Client, &Address, 3, 0);

    if(Server == NULL) 
    {
        enet_host_destroy(Client);
        Client = NULL;
        return false;
    }

    PlayerPacket announce = { 0 };
    announce.Command = static_cast<int>(NetworkCommands::UpdateInput);
    CopySafeName(announce.Name, sizeof(announce.Name), LocalPlayerName);
    announce.Position = (Vector3){ 0.0f, 0.0f, 0.0f };
    announce.Rotation = MatrixIdentity();

    ENetPacket* packet = enet_packet_create(&announce, sizeof(announce), 0);
    enet_peer_send(Server, 2, packet);

    return true;
}

void NetClient::BeginHostedSession(const char* playerName)
{
    CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), playerName);
    if (LocalPlayerName[0] == '\0')
        CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), "Player");

    LocalPlayerId = 0;
    Players[0].Active = true;
    Players[0].Position = (Vector3){ 0.0f, 0.0f, 0.0f };
    Players[0].Rotation = MatrixIdentity();
    CopySafeName(Players[0].Name, sizeof(Players[0].Name), LocalPlayerName);
    CopySafeName(PlayerNames[0], sizeof(PlayerNames[0]), LocalPlayerName);
    LastInputSend = -UPDATE_INTERVAL;
}

// A new remote player was added to our local simulation
void NetClient::HandleAddPlayer(PlayerPacket packet)
{
    printf("Added new player\n");
	// find out who the server is talking about
	int remotePlayer = packet.Id;

    // skip if out of bounds, or local player
	if(remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId)
		return;

	// set them as active and update the location
	Players[remotePlayer].Active = true;
    CopySafeName(Players[remotePlayer].Name, sizeof(Players[remotePlayer].Name), packet.Name);
    CopySafeName(PlayerNames[remotePlayer], sizeof(PlayerNames[remotePlayer]), packet.Name);

	Players[remotePlayer].Position = packet.Position;
    Players[remotePlayer].TargetPosition = packet.Position;
	Players[remotePlayer].Rotation = packet.Rotation;
	Players[remotePlayer].LastUpdateTime = LastNow;
}

// A remote player has left the game and needs to be removed from the local simulation
void NetClient::HandleRemovePlayer(PlayerPacket packet)
{
	// find out who the server is talking about
	int remotePlayer = packet.Id;

    // skip if out of bounds, or local player
	if(remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId)
		return;

	// remove the player from the simulation. No other data is needed except the player id
	Players[remotePlayer].Active = false;
    Players[remotePlayer].Name[0] = '\0';
    PlayerNames[remotePlayer][0] = '\0';
}

// The server has a new position for a player in our local simulation
void NetClient::HandleUpdatePlayer(PlayerPacket packet)
{
	// find out who the server is talking about
	int remotePlayer = packet.Id;

    // skip if out of bounds, local player, or not active
	if(remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId || !Players[remotePlayer].Active)
		return;

	// update the last known position and movement
    CopySafeName(Players[remotePlayer].Name, sizeof(Players[remotePlayer].Name), packet.Name);
    CopySafeName(PlayerNames[remotePlayer], sizeof(PlayerNames[remotePlayer]), packet.Name);

	Players[remotePlayer].TargetPosition = packet.Position;
    Players[remotePlayer].Rotation = packet.Rotation;
	Players[remotePlayer].LastUpdateTime = LastNow;
}

void NetClient::UpdateLocalPlayer(Vector3 pos, Matrix rot)
{
    // if we are not accepted, we can't update
	if(LocalPlayerId < 0)
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
    for(int i = 0; i < packet.AsteroidCount && i < MAX_ASTEROIDS; i++) {
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

bool NetClient::GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot, float* scale)
{   
    // skip if out of bounds
    if(id < 0 || id >= AsteroidAmount)
    {
        return false;
    }

    *pos = Asteroids[id].Position;
    *rot = Asteroids[id].Rotation;
    if (scale != nullptr)
        *scale = Asteroids[id].Scale;
    return true;
}

void NetClient::HandleUpdateScoreboard(ScoreboardPacket packet)
{
    // copy argument into our scoreboard
    memcpy(Scoreboard, packet.Scoreboard, sizeof(packet.Scoreboard));
    for (int i = 0; i < MAX_PLAYERS; ++i)
        CopySafeName(PlayerNames[i], sizeof(PlayerNames[i]), packet.Names[i]);
}


void NetClient::NetUpdate(double now, float delta)
{
    LastNow = now;

    // Skip if no server
    if (Server == NULL)
        return;

    for (int i = 0; i < AsteroidAmount; i++)
    {
        Asteroids[i].Position.x += Asteroids[i].Velocity.x * delta;
        Asteroids[i].Position.y += Asteroids[i].Velocity.y * delta;
        Asteroids[i].Position.z += Asteroids[i].Velocity.z * delta;
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (i != LocalPlayerId && Players[i].Active)
        {
            float lerpSpeed = 10.0f; // Higher = snappier, Lower = floatier
            Players[i].Position.x += (Players[i].TargetPosition.x - Players[i].Position.x) * lerpSpeed * delta;
            Players[i].Position.y += (Players[i].TargetPosition.y - Players[i].Position.y) * lerpSpeed * delta;
            Players[i].Position.z += (Players[i].TargetPosition.z - Players[i].Position.z) * lerpSpeed * delta;
        }
    }

    // if we're in a server send packet to server
    if(LocalPlayerId >= 0 && now - LastInputSend > UPDATE_INTERVAL)
    {
        // construct a buffer and send it through a packet
        PlayerPacket buffer = { 0 };
        buffer.Command = NetworkCommands::UpdateInput;
        CopySafeName(buffer.Name, sizeof(buffer.Name), LocalPlayerName);
        buffer.Position = Players[LocalPlayerId].Position;
        buffer.Rotation = Players[LocalPlayerId].Rotation;

        // create packet
        ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
        // send the data to the server
        enet_peer_send(Server, 2, packet);

        LastInputSend = now;
    }

    ENetEvent event = {};
    if(enet_host_service(Client, &event, 0) > 0)
    {
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
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
                            case NetworkCommands::AddPlayer:
                                HandleAddPlayer(recieved);
                                break;

                            case NetworkCommands::RemovePlayer:
                                HandleRemovePlayer(recieved);
                                break;

                            case NetworkCommands::UpdatePlayer:
                                HandleUpdatePlayer(recieved);
                                break;
                        }
                    }
                    // We do not have an ID in the server, so we need to read the accept command
                    else 
                    {
                        if (recieved.Command != NetworkCommands::AcceptPlayer)
                            return;

                        // Read id from command, check if in bounds, and prepare it to be in the game.
                        LocalPlayerId = recieved.Id;

                        if (LocalPlayerId < 0 || LocalPlayerId > MAX_PLAYERS)
                        {
                            LocalPlayerId = -1;
                            break;
                        }

                        if (recieved.Name[0] != '\0')
                            CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), recieved.Name);
                        else if (LocalPlayerName[0] == '\0')
                            CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), "Player");
                        CopySafeName(PlayerNames[LocalPlayerId], sizeof(PlayerNames[LocalPlayerId]), LocalPlayerName);

                        LastInputSend = -UPDATE_INTERVAL;
                        
                        Players[LocalPlayerId].Active = true;
                        CopySafeName(Players[LocalPlayerId].Name, sizeof(Players[LocalPlayerId].Name), LocalPlayerName);
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
                printf("[CLIENT] DISCONNECT_TIMEOUT event: Connection timeout\n");
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
            case ENET_EVENT_TYPE_DISCONNECT:
                printf("[CLIENT] DISCONNECT event: Server disconnected\n");
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
            case ENET_EVENT_TYPE_NONE:
            break;
        }
    }
}

