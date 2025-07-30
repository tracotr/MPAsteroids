#define ENET_IMPLEMENTATION
#include "include/net_common.h"
#include "include/PlayerManager.h"
#include "include/AsteroidManager.h"
#include "include/NetworkUtil.h"


#include <stdio.h>
#include <vector>
#include <ctime>
#include <random>


ENetAddress address = { 0 };
ENetHost* server = { 0 };


double GetTime()
{
    return (double)clock() / CLOCKS_PER_SEC;
}


int main()
{
    if (enet_initialize () != 0)
    {
        printf("An error occurred while initializing ENet.\n");
        return 1;
    }

    PlayerManager playerManager;
    AsteroidManager asteroidManager;

	address.host = ENET_HOST_ANY;
	address.port = SERVER_PORT;

    // create the server host
	server = enet_host_create(&address, MAX_PLAYERS, 2, 0, 0);
    
    if (server == NULL) 
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
            asteroidManager.UpdateAsteroids(playerManager.Players, delta);
            asteroidManager.lastAsteroidUpdateTime = now;
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
                    int playerId = playerManager.InitializeNewPlayer(&event);
                    
                    if(playerId == -1){
                        break;
                    }

                    // Spawn new asteroids 
                    int asteroidAmount = 32;
                    asteroidManager.SpawnAsteroids(asteroidAmount);

                    // send new player info about all asteroids
                    AsteroidInfoPacket asteroid_buffer = { 0 };
                    asteroid_buffer.Command = AddAsteroid;
                    AsteroidInfo (&curAsteroids)[MAX_ASTEROIDS] = asteroidManager.GetAsteroids();

                    memcpy(asteroid_buffer.AllAsteroids, curAsteroids, sizeof(curAsteroids));
                    asteroid_buffer.AsteroidCount = asteroidManager.GetAsteroidAmount();
                    ENetPacket* asteroid_packet = enet_packet_create(&asteroid_buffer, sizeof(asteroid_buffer), ENET_PACKET_FLAG_RELIABLE);
                    NetworkUtil::SendPacketToOnly(asteroid_packet, playerId, 1);
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

                        if (command == UpdateInput)
                        {
                            playerManager.UpdateInput(&received, playerId);
                        }
                    }
                    else if(event.packet->dataLength == sizeof(AsteroidDestroyPacket))
                    {
                        AsteroidDestroyPacket received;
                        memcpy(&received, event.packet->data, sizeof(AsteroidDestroyPacket));
                        asteroidManager.RespawnAsteroid(received.AsteroidID);
                    }

                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                case ENET_EVENT_TYPE_DISCONNECT:
                {
                    printf("player disconnected\n");

                    int playerId = playerManager.GetPlayerId(event.peer);
                    if (playerId == -1)
                    {
                        break;
                    }

                    playerManager.DisconnectPlayer(playerId);

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