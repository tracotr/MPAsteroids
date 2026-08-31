#pragma once

#include "../raylib/raymath.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#define MAX_PLAYERS 256

// Most asteroids the server will ever hold at once.
#define MAX_ASTEROIDS 256

// The play area is a set of cubes this wide, switching on around each player.
#define REGION_CELL_SIZE 35.0f

// Cells switch off again after this long with nobody inside.
#define REGION_CELL_TTL 12.0

// Ceiling on how many cubes can be switched on at once.
#define MAX_REGION_CELLS 64

// Asteroids per cube, so the field stays as dense however large the world grows.
#define ASTEROID_PER_CELL 5
#define ASTEROID_MAX_TOTAL 160

// An asteroid being put back into play looks for a spot at least this far from every player.
#define ASTEROID_PLACEMENT_CLEARANCE 18.0f

// An asteroid outside the world is left alone until it is at least this far from the nearest player.
#define ASTEROID_RELOCATE_DISTANCE 60.0f

// Past this the client stops drawing an asteroid.
#define ASTEROID_DRAW_DISTANCE 85.0f

// The edge of the world. Ships cannot fly past this.
#define WORLD_RADIUS 260.0f

// How far out players appear, kept just inside the world edge.
#define PLAYER_SPAWN_RADIUS 240.0f

#define MAX_SQR_V3 3.402823466e+38F
#define MAX_PLAYER_NAME_LENGTH 32

// The smallest asteroid size
#define MIN_ASTEROID_SCALE 0.45f
#define ASTEROID_SPLIT_FACTOR 0.68f

// A rock's health, from its size
#define ASTEROID_HEALTH_PER_AREA 190.0f

inline float AsteroidHealthForScale(float scale)
{
    return ASTEROID_HEALTH_PER_AREA * scale * scale;
}

// Port the server binds. Behind a reverse proxy this stays private to the host.
#ifndef SERVER_PORT
#define SERVER_PORT 25665
#endif

#ifndef SERVER_HOST
#define SERVER_HOST "127.0.0.1"
#endif

// Port the browser dials, which behind a proxy is the public HTTPS port rather than SERVER_PORT.
#ifndef SERVER_PUBLIC_PORT
#define SERVER_PUBLIC_PORT SERVER_PORT
#endif

// URL path the proxy routes to the server, letting the site and game share a domain.
#ifndef SERVER_PATH
#define SERVER_PATH "/"
#endif

// Players past this are still scored, just not all listed at once.
#define LEADERBOARD_VISIBLE_ROWS 8

// Players report in twenty times a second.
#define PLAYER_STALE_SECONDS 2.0

// How long after a kill that player cannot be killed again.
#define KILL_COOLDOWN_SECONDS 3.0

// Levelling stops here. Reaching it takes long enough that the cap is a formality.
#define MAX_LEVEL 30

// One pick per level, so the history can never outgrow the level cap.
#define MAX_UPGRADE_PICKS MAX_LEVEL

// Cards shown at a level up. Bound to the 1, 2 and 3 keys.
#define UPGRADE_OFFER_COUNT 3

enum NetworkCommands
{
    AcceptPlayer = 1,
    AddPlayer = 2,
    RemovePlayer = 3,
    UpdatePlayer = 4,
    UpdateInput = 5,
    UpdateAsteroid = 6,
    HitAsteroid = 7,
    UpdateScoreboard = 8,
    ResetScoreboardId = 9,
    // Retired. One packet per laser could not carry a thirty-laser shotgun.
    RetiredFireLaser = 10,
    PlayerKilled = 11,
    PlayerHit = 12,
    AsteroidCollision = 13,
    UpdateUpgrades = 14,
    ChooseUpgrade = 15,
    UpdateHealth = 16,
    FireVolley = 17,
};

#pragma pack(push, 1)

struct PlayerPacket
{
    int Command;
    int Id;
    bool Active;
    char Name[MAX_PLAYER_NAME_LENGTH];
    Vector3 Position;
    Matrix Rotation;

    // Stamped by the server on the way out, ignored on the way in.
    float Health;
    float MaxHealth;
    uint8_t Level;

    // Which chassis to draw. Weapon branches need nothing here.
    uint8_t Evolution;
};

// 40 bytes per asteroid. No rotation matrix on purpose: the client works the spin out from Seed.
struct AsteroidInfo
{
    // Stable for an asteroid's whole life and never reused.
    uint32_t Id;
    uint8_t Seed;
    uint8_t Padding[3];
    Vector3 Position;
    Vector3 Velocity;
    float Scale;

    // What is left of it. Sent because a rock no longer dies to the first thing that touches it.
    float Health;
};

// Only the first AsteroidCount entries are actually sent, so the packet varies in length.
struct AsteroidInfoPacket
{
    int Command;
    int AsteroidCount;
    AsteroidInfo Asteroids[MAX_ASTEROIDS];
};

// One laser landing on one rock.
struct AsteroidHitPacket
{
    int Command;
    int PlayerID;
    uint32_t AsteroidId;
};

struct ScoreboardPacket
{
    int Command;
    int Scoreboard[MAX_PLAYERS];
    char Names[MAX_PLAYERS][MAX_PLAYER_NAME_LENGTH];
    int Id;
};

// Sent by the shooter, who tests the laser exactly as it is drawn on their screen.
struct PlayerKillPacket
{
    int Command;
    int KillerId;
    int VictimId;
};

// One whole trigger pull, however many lasers it becomes.
struct VolleyPacket
{
    int Command;
    int PlayerID;

    // Where the ship was and which way it faced.
    Vector3 Position;
    Vector3 Forward;
    Vector3 Up;

    // Stamped by the server from the shooter's build.
    float Speed;
    float Radius;
    float Lifetime;

    // Which weapon to lay the pattern out from, and which pull this is.
    uint8_t WeaponId;
    uint8_t VolleyIndex;
    uint8_t Padding[2];
};

// Sent by the shooter. The damage is not in here: the server looks it up from the shooter's own build.
struct PlayerHitPacket
{
    int Command;
    int VictimId;
};

// Self-reported: we ran into a rock.
struct AsteroidCollisionPacket
{
    int Command;
    uint32_t AsteroidId;
};

// One player's progress and build, sent only to them and only when it changes.
struct UpgradeStatePacket
{
    int Command;
    int Xp;
    int Level;
    int PendingPicks;
    int HistoryCount;
    uint8_t Offered[UPGRADE_OFFER_COUNT];
    uint8_t OfferCount;
    uint8_t History[MAX_UPGRADE_PICKS];
    uint8_t Padding[2];
};

// Our own health. Position updates go to everyone except the player they describe.
struct PlayerHealthPacket
{
    int Command;
    float Health;
    float MaxHealth;
};

// The player taking one of the three they were shown.
struct UpgradeChoosePacket
{
    int Command;
    uint8_t UpgradeId;
    uint8_t Padding[3];
};

#pragma pack(pop)

static_assert(ASTEROID_MAX_TOTAL <= MAX_ASTEROIDS, "asteroid field must fit the pool");
static_assert(sizeof(AsteroidInfo) == 40, "AsteroidInfo is a wire format; update the size noted above");

inline size_t AsteroidPacketSize(int count)
{
    return offsetof(AsteroidInfoPacket, Asteroids) + (size_t)count * sizeof(AsteroidInfo);
}

// Every packet starts with its command, so the type is read rather than guessed from the length.
inline int PeekCommand(const void* data, size_t length)
{
    if (length < sizeof(int))
        return -1;

    int command = 0;
    memcpy(&command, data, sizeof(int));
    return command;
}
