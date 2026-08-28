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

    // Our copy of one of the server's asteroids. Position is the one we draw,
    // and it slides gradually toward ServerPosition: where the server last said
    // it was, carried forward at its last known speed. Keeping the two separate
    // is what stops each update from jerking the asteroid sideways.
    struct ClientAsteroid
    {
        uint32_t Id = 0;
        Vector3 Position = { 0.0f, 0.0f, 0.0f };
        Vector3 ServerPosition = { 0.0f, 0.0f, 0.0f };
        Vector3 Velocity = { 0.0f, 0.0f, 0.0f };
        float Scale = 1.0f;

        Vector3 SpinAxis = { 0.0f, 1.0f, 0.0f };
        float SpinSpeed = 0.0f;
        float SpinAngle = 0.0f;

        // Set when we report a hit, so the asteroid disappears immediately
        // instead of lingering until the server's next broadcast. Expires on its
        // own if the server declines the hit.
        double DestroyReportedAt = -1.0;
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

    // How quickly the drawn position catches up to the server's, per second.
    static constexpr float ASTEROID_CORRECTION_RATE = 9.0f;
    static constexpr float PLAYER_CORRECTION_RATE = 12.0f;

    // A gap bigger than this means the server moved the asteroid to the far
    // side, not that our guess drifted slightly, so we jump straight to it.
    // Sliding across a gap that large would look like the rock flying away.
    static constexpr float ASTEROID_SNAP_DISTANCE = 6.0f;

    // How long a locally reported hit hides the asteroid before it is trusted
    // back into the world.
    static constexpr double DESTROY_REPORT_GRACE = 0.4;

    // The asteroids that currently exist, packed at the front. Their order
    // changes from one update to the next, so anything kept for longer than a
    // frame has to refer to them by Id instead of by position in this list.
    ClientAsteroid Asteroids[MAX_ASTEROIDS];
    int AsteroidCount = 0;

    // Set when the server reports us killed, and cleared once the world has
    // acted on it, so a single death cannot respawn us twice.
    bool killedPending = false;
    int killedBy = -1;

    NetStatus Status = NetStatus::Disconnected;

    void DispatchPacket(const uint8_t* data, size_t length);
    void SendPacket(const void* data, size_t length);
    void ApplyAsteroidSnapshot(const AsteroidInfoPacket& packet, int count);

public:
    struct RemoteProjectileEvent {
        int PlayerId;
        Vector3 Position;
        Vector3 Velocity;

        // When the packet landed. Socket callbacks keep firing while a tab is in
        // the background but the frame loop does not, so shots pile up here and
        // would otherwise all appear at once, at their original positions, the
        // moment the tab comes back.
        double ArrivalTime;
    };

    RemoteProjectileEvent RemoteProjectilesQueue[MAX_PROJECTILES];
    int RemoteProjectileCount = 0;

    void HandleAddPlayer(PlayerPacket packet);
    void HandleRemovePlayer(PlayerPacket packet);
    void HandleUpdatePlayer(PlayerPacket packet);
    void UpdateLocalPlayer(Vector3 pos, Matrix rot);
    bool GetPlayerSpatial(int id, Vector3* pos, Matrix* Rotation);

    // Their client has stopped reporting in, so they are not a valid target.
    bool IsPlayerStale(int id) const;

    // How long a queued shot has been waiting, in seconds.
    double QueuedProjectileAge(int index) const;
    void HandlePlayerCollision();

    // Reports shooting another player: clears their score and credits ours.
    void ReportKill(int victimId);

    // True once when the server says we were shot. Returns the shooter's id.
    bool ConsumeKilled(int* killerId);

    // index is a per-frame slot, valid only until the next NetUpdate.
    bool GetAsteroidSpatial(int index, Vector3* pos, Matrix* rot, float* scale = nullptr);
    uint32_t GetAsteroidId(int index) const;

    // Reports the hit and hides the asteroid locally straight away.
    void ReportAsteroidDestroyed(uint32_t asteroidId);

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
    int GetAsteroidCount() const { return AsteroidCount; };
    int (&GetScoreboard())[MAX_PLAYERS] { return Scoreboard; };
    char (&GetPlayerNames())[MAX_PLAYERS][MAX_PLAYER_NAME_LENGTH] { return PlayerNames; }
};
