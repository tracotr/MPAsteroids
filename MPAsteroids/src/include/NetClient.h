#pragma once

#include "raylib/raymath.h"
#include "networking/NetConstants.h"

#include <cstdint>
#include <stdio.h>

#define MAX_PROJECTILES 100

enum class NetStatus
{
    Disconnected,
    Connecting,
    Connected
};

class NetClient
{
private:
    struct RemotePlayers
    {
        bool Active = false;
        char Name[MAX_PLAYER_NAME_LENGTH] = { 0 };

        Vector3 Position;
        Vector3 TargetPosition;
        Matrix Rotation;

        double LastUpdateTime;
    };

    int LocalPlayerId = -1;
    RemotePlayers Players[MAX_PLAYERS] = { 0 };

    char LocalPlayerName[MAX_PLAYER_NAME_LENGTH] = { 0 };
    char PlayerNames[MAX_PLAYERS][MAX_PLAYER_NAME_LENGTH] = { 0 };
    int Scoreboard[MAX_PLAYERS] = { 0 };

    double LastNow = 0;
    double LastInputSend = -100;

    // Outbound input rate: 20 sends a second, independent of framerate.
    const double UPDATE_INTERVAL = 1.0f / 20.0f;

    AsteroidInfo Asteroids[MAX_ASTEROIDS];
    int AsteroidAmount = 0;

    NetStatus Status = NetStatus::Disconnected;

    void DispatchPacket(const uint8_t* data, size_t length);
    void SendPacket(const void* data, size_t length);

public:
    struct RemoteProjectileEvent {
        Vector3 Position;
        Vector3 Velocity;
    };

    RemoteProjectileEvent RemoteProjectilesQueue[MAX_PROJECTILES];
    int RemoteProjectileCount = 0;

    void HandleAddPlayer(PlayerPacket packet);
    void HandleRemovePlayer(PlayerPacket packet);
    void HandleUpdatePlayer(PlayerPacket packet);
    void UpdateLocalPlayer(Vector3 pos, Matrix rot);
    bool GetPlayerSpatial(int id, Vector3* pos, Matrix* Rotation);
    void HandlePlayerCollision();

    void HandleUpdateAsteroid(AsteroidInfoPacket packet);
    void HandleDestroyAsteroid(int playerIdx, int asteroidIdx);
    bool GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot, float* scale = nullptr);

    void HandleUpdateScoreboard(ScoreboardPacket packet);
    void SendProjectile(Vector3 position, Vector3 velocity);

    bool NetConnect(const char* address, const char* playerName = nullptr);
    void NetDisconnect();
    void NetUpdate(double now, float delta);

    // Called from the WebSocket event callbacks.
    void OnSocketOpen();
    void OnSocketClosed();
    void OnSocketMessage(const uint8_t* data, size_t length);

    NetStatus GetStatus() const { return Status; }
    int GetLocalPlayerId() { return LocalPlayerId; };
    const char* GetLocalPlayerName() const { return LocalPlayerName; }
    const char* GetPlayerName(int id) const { return (id >= 0 && id < MAX_PLAYERS) ? PlayerNames[id] : ""; }
    int GetMaxAsteroids() { return AsteroidAmount; };
    int (&GetScoreboard())[MAX_PLAYERS] { return Scoreboard; };
    char (&GetPlayerNames())[MAX_PLAYERS][MAX_PLAYER_NAME_LENGTH] { return PlayerNames; }
};
