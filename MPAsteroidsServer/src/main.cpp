#define ENET_IMPLEMENTATION

#include "include/raylib/raymath.h"
#include "include/PlayerManager.h"
#include "include/AsteroidManager.h"
#include "include/ScoreboardManager.h"
#include "include/NetworkUtil.h"
#include "include/networking/NetCommon.h"

#include <ctime>
#include <string>

ENetAddress Address = { 0 };
ENetHost* Server = { 0 };

double GetTime()
{
    return (double)clock() / CLOCKS_PER_SEC;
}

int main()
{
    if (enet_initialize () != 0)
    {
        NetworkUtil::Log("An error occurred while initializing ENet.");
        return 1;
    }

    PlayerManager playerManager;
    AsteroidManager asteroidManager;
    ScoreboardManager scoreboardManager;

	Address.host = ENET_HOST_ANY;
	Address.port = SERVER_PORT;

    // create the server host
    // channel 0: reliable packets
    // channel 1: asteroid updates
    // channel 2: player updates
	Server = enet_host_create(&Address, MAX_PLAYERS, 3, 0, 0);
    
    if (Server == NULL) 
    {
        NetworkUtil::Log("An error occurred while trying to create an ENet server host.");
        return 1;
    }
    
    // server run successful, run server loop
    NetworkUtil::Log("Server Running Successfully");

    bool run = true;
	while (run)
	{
        double now = GetTime();
        double delta = now - asteroidManager.lastAsteroidUpdateTime;

        // Only update asteroid movement if enough time has passed
        if (delta >= asteroidManager.ASTEROID_TICK_INTERVAL)
        {
            asteroidManager.UpdateAsteroids(playerManager.GetPlayers(), delta);
            asteroidManager.lastAsteroidUpdateTime = now;
        }

        // Check for events (updates from clients)
        ENetEvent event = {};
        if (enet_host_service(Server, &event, 0) > 0)
		{
            switch(event.type)
            {   
                // Someone new joined
                case ENET_EVENT_TYPE_CONNECT:
                {
                    int playerId = playerManager.InitializeNewPlayer(&event);
                    
                    if(playerId == -1){
                        break;
                    }

                    // Spawn new asteroids 
                    int asteroidAmount = 10;
                    asteroidManager.SpawnAsteroids(asteroidAmount);

                    // send player current scoreboard data
                    scoreboardManager.UpdateScoreboard(playerManager.GetPlayers());
                    break;
                }
                
                case ENET_EVENT_TYPE_RECEIVE:
                {
                    if(event.packet->dataLength == sizeof(PlayerPacket))
                    {
                        int playerId = playerManager.GetPlayerId(event.peer);

                        // boot event if not in our list of active players
                        if (playerId == -1)
                        {
                            enet_peer_disconnect(event.peer, 0);
                            break;
                        }
                        
                        PlayerPacket received;
                        memcpy(&received, event.packet->data, sizeof(PlayerPacket));

                        NetworkCommands command = (NetworkCommands)received.Command;

                        if(command == NetworkCommands::UpdateInput)
                        {
                            playerManager.UpdateInput(&received, playerId);
                        }
                    }
                    else if(event.packet->dataLength == sizeof(AsteroidDestroyPacket))
                    {
                        AsteroidDestroyPacket received;
                        memcpy(&received, event.packet->data, sizeof(AsteroidDestroyPacket));

                        NetworkCommands command = (NetworkCommands)received.Command;

                        if(command == NetworkCommands::DestroyAsteroid)
                        {
                            asteroidManager.RespawnAsteroid(received.AsteroidID);
                            scoreboardManager.AddScoreId(received.PlayerID, 5);
                            scoreboardManager.UpdateScoreboard(playerManager.GetPlayers());
                        }
                    }
                    else if(event.packet->dataLength == sizeof(ScoreboardPacket))
                    {
                        ScoreboardPacket received;
                        memcpy(&received, event.packet->data, sizeof(ScoreboardPacket));

                        NetworkCommands command = (NetworkCommands)received.Command;
                        if(command == NetworkCommands::ResetScoreboardId)
                        {
                            scoreboardManager.ResetScoreId(received.Id);
                            scoreboardManager.UpdateScoreboard(playerManager.GetPlayers());
                        }
                    }

                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    int playerId = playerManager.GetPlayerId(event.peer);
                    
                    if (playerId == -1)
                    {
                        break;
                    }

                    NetworkUtil::Log(std::to_string(playerId) + " has disconnected.");

                    playerManager.DisconnectPlayer(playerId);
                    scoreboardManager.ResetScoreId(playerId);
                    break;
                }
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }
    
    enet_host_destroy(Server);
    enet_deinitialize();

    return 0;
}