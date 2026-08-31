#pragma once

#include "raylib/raymath.h"
#include "networking/NetConstants.h"
#include "Upgrades.h"

#include <cstdint>
#include <stdio.h>

// Sized for a lobby firing the wide end of the tree.
#define MAX_LASERS 2048

// Slots only our own lasers may take.
#define LOCAL_LASER_RESERVE 256

// Trigger pulls that can arrive between two frames.
#define MAX_REMOTE_VOLLEYS 128

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

        // All stamped by the server.
        float Health = 0.0f;
        float MaxHealth = 0.0f;
        uint8_t Level = 0;
        uint8_t Evolution = 0;

        double LastUpdateTime;
    };

    // Our copy of a server asteroid.
    struct ClientAsteroid
    {
        uint32_t Id = 0;
        Vector3 Position = { 0.0f, 0.0f, 0.0f };
        Vector3 ServerPosition = { 0.0f, 0.0f, 0.0f };
        Vector3 Velocity = { 0.0f, 0.0f, 0.0f };
        float Scale = 1.0f;

        // What the server last said was left.
        float Health = 0.0f;

        Vector3 SpinAxis = { 0.0f, 1.0f, 0.0f };
        float SpinSpeed = 0.0f;
        float SpinAngle = 0.0f;

        // Set when we report a hit, so the asteroid disappears immediately.
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

    // A gap this big means the server moved the asteroid rather than our guess drifting, so we jump.
    static constexpr float ASTEROID_SNAP_DISTANCE = 6.0f;

    // How long a locally reported hit hides the asteroid before it is trusted back into the world.
    static constexpr double DESTROY_REPORT_GRACE = 0.4;

    // The asteroids that exist, packed at the front.
    ClientAsteroid Asteroids[MAX_ASTEROIDS];
    int AsteroidCount = 0;

    // Our build, mirrored from the server.
    UpgradeState Upgrades;

    // Our own health. It cannot ride on the position updates the way every other ship's does.
    float LocalHealth = 100.0f;
    float LocalMaxHealth = 100.0f;

    // Set when the server reports us killed, and cleared once the world has acted on it.
    bool killedPending = false;
    int killedBy = -1;

    NetStatus Status = NetStatus::Disconnected;

    void DispatchPacket(const uint8_t* data, size_t length);
    void SendPacket(const void* data, size_t length);
    void ApplyAsteroidSnapshot(const AsteroidInfoPacket& packet, int count);

    // Hides an asteroid straight away without telling the server anything.
    void HideAsteroidLocally(uint32_t asteroidId);

public:
    // Somebody else's trigger pull, waiting for the next frame to lay it out.
    struct RemoteVolleyEvent {
        int PlayerId;

        Vector3 Position;
        Vector3 Forward;
        Vector3 Up;

        float Speed;
        float Radius;
        float Lifetime;

        uint8_t WeaponId;
        uint8_t VolleyIndex;

        // When the packet landed. Sockets keep firing while a tab is in the background but frames do not.
        double ArrivalTime;
    };

    // Volleys rather than lasers, so one shotgun blast takes one slot.
    RemoteVolleyEvent RemoteVolleyQueue[MAX_REMOTE_VOLLEYS];
    int RemoteVolleyCount = 0;

    void HandleAddPlayer(PlayerPacket packet);
    void HandleRemovePlayer(PlayerPacket packet);
    void HandleUpdatePlayer(PlayerPacket packet);
    void UpdateLocalPlayer(Vector3 pos, Matrix rot);
    bool GetPlayerSpatial(int id, Vector3* pos, Matrix* Rotation);

    // Their client has stopped reporting in, so they are not a valid target.
    bool IsPlayerStale(int id) const;

    // How long a queued volley has been waiting, in seconds.
    double QueuedVolleyAge(int index) const;

    // Reports landing a laser on another player.
    void ReportHit(int victimId);

    // Reports running into an asteroid, and hides the rock locally.
    void ReportAsteroidCollision(uint32_t asteroidId);

    // Takes one of the cards the server has offered us.
    void SendUpgradeChoice(uint8_t upgradeId);

    // True once when the server says we were laser. Returns the shooter's id.
    bool ConsumeKilled(int* killerId);

    // index is a per-frame slot, valid only until the next NetUpdate.
    bool GetAsteroidSpatial(int index, Vector3* pos, Matrix* rot, float* scale = nullptr);
    uint32_t GetAsteroidId(int index) const;

    // Reports one laser landing on a rock and takes that much off our own copy.
    bool ReportAsteroidHit(uint32_t asteroidId, float damage);

    void HandleUpdateScoreboard(ScoreboardPacket packet);

    // Reports one trigger pull. What it becomes is worked out from the weapon by whoever receives it.
    void SendVolley(Vector3 position, Vector3 forward, Vector3 up, int volleyIndex);

    bool NetConnect(const char* address, const char* playerName = nullptr);
    void NetDisconnect();
    void NetUpdate(double now, float delta);

    // Called from the WebSocket event callbacks.
    void OnSocketOpen();
    void OnSocketClosed();
    void OnSocketMessage(const uint8_t* data, size_t length);

    NetStatus GetStatus() const { return Status; }
    int GetLocalPlayerId() { return LocalPlayerId; };

    // Everything the ship is currently capable of.
    const UpgradeState& GetUpgrades() const { return Upgrades; }
    const ShipStats& GetStats() const { return Upgrades.Stats(); }

    float GetHealth() const { return LocalHealth; }
    float GetMaxHealth() const { return LocalMaxHealth; }

    // Another ship's health, for drawing a bar over it.
    bool GetPlayerHealth(int id, float* health, float* maxHealth) const;
    uint8_t GetPlayerEvolution(int id) const;
    int GetPlayerLevel(int id) const;
    int GetAsteroidCount() const { return AsteroidCount; };
    int (&GetScoreboard())[MAX_PLAYERS] { return Scoreboard; };
    char (&GetPlayerNames())[MAX_PLAYERS][MAX_PLAYER_NAME_LENGTH] { return PlayerNames; }
};
