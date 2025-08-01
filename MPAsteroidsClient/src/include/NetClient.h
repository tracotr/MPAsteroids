#pragma once
#define ENET_IMPLEMENTATION

#include "raylib/raymath.h"

#include "networking/NetConstants.h"

#include <stdio.h>

class NetClient
{
private:
    struct RemotePlayers
    {
        bool Active = false;

        Vector3 Position;
        Matrix Rotation;

        double LastUpdateTime;
    };



    int LocalPlayerId = -1;
    RemotePlayers Players[MAX_PLAYERS] = { 0 };

    int Scoreboard[MAX_PLAYERS] = { 0 };

    // last time we updated
    double LastNow = 0; 
    // how long in seconds since the last time we sent an update
    double LastInputSend = -100;

    // how long to wait between updates (20 update ticks a second)
    const double UPDATE_INTERVAL = 1.0f / 20.0f;

    AsteroidInfo Asteroids[MAX_ASTEROIDS];
    int AsteroidAmount = 0;

public:
    void HandleAddPlayer(PlayerPacket packet);
    void HandleRemovePlayer(PlayerPacket packet);
    void HandleUpdatePlayer(PlayerPacket packet);
    void UpdateLocalPlayer(Vector3 pos, Matrix rot);
    bool GetPlayerSpatial(int id, Vector3* pos, Matrix* Rotation);
    void HandlePlayerCollision();

    void HandleUpdateAsteroid(AsteroidInfoPacket packet);
    void HandleDestroyAsteroid(int playerIdx, int asteroidIdx);
    bool GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot);

    void HandleUpdateScoreboard(ScoreboardPacket packet);

    void NetConnect(const char* address);
    void NetUpdate(double now, float delta);

    int GetLocalPlayerId() { return LocalPlayerId; };
    int GetMaxAsteroids() { return AsteroidAmount; };
    int (&GetScoreboard())[MAX_PLAYERS] { return Scoreboard; };
};

