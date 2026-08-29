#pragma once

#include "../raylib/raymath.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#define MAX_PLAYERS 64

// Most asteroids the server will ever hold at once. The usual number is far
// lower; the spare room is there so a burst of splits always has space.
#define MAX_ASTEROIDS 256

// The play area is a set of cubes this wide, switching on around each player, so
// going somewhere new switches more on.
#define REGION_CELL_SIZE 35.0f

// Cells switch off again after this long with nobody inside, which keeps the
// world roughly the size of the space actually being used.
#define REGION_CELL_TTL 12.0

// Ceiling on how many cubes can be switched on at once.
#define MAX_REGION_CELLS 64

// Asteroids per cube, so the field stays as dense however large the world grows.
// The total ceiling is low enough to keep the broadcast small.
#define ASTEROID_PER_CELL 5
#define ASTEROID_MAX_TOTAL 160

// An asteroid being put back into play looks for a spot at least this far from
// every player, so one never drops into somebody's lap.
#define ASTEROID_PLACEMENT_CLEARANCE 18.0f

// An asteroid outside the world is left alone until it is at least this far from
// the nearest player, so nobody watches one blink across the sky.
#define ASTEROID_RELOCATE_DISTANCE 60.0f

// Past this the client stops drawing an asteroid. Comfortably beyond the
// relocate distance, so nothing vanishes while it is still worth looking at.
#define ASTEROID_DRAW_DISTANCE 85.0f

// How far out players appear, spread around the centre rather than stacked on it.
// Far enough that a respawn does not drop you back into the fight you just lost.
#define PLAYER_SPAWN_RADIUS 50.0f

#define MAX_SQR_V3 3.402823466e+38F
// Includes the terminator. It is a field in PlayerPacket, so changing it changes
// the wire format and both halves have to be rebuilt together.
#define MAX_PLAYER_NAME_LENGTH 32

// The smallest piece worth making. An asteroid shatters instead of splitting
// once shrinking it by ASTEROID_SPLIT_FACTOR would take it below this.
#define MIN_ASTEROID_SCALE 0.45f
#define ASTEROID_SPLIT_FACTOR 0.68f

// A rock's hull, from its size. Squared because a rock is an area, not a length:
// four shots for the biggest that spawns, one for most fragments.
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

// Port the browser dials, which behind a proxy is the public HTTPS port rather
// than SERVER_PORT: the proxy accepts 443 and forwards to the server's local port.
#ifndef SERVER_PUBLIC_PORT
#define SERVER_PUBLIC_PORT SERVER_PORT
#endif

// URL path the proxy routes to the server, letting the site and game share a domain.
#ifndef SERVER_PATH
#define SERVER_PATH "/"
#endif

// Players past this are still scored, just not all listed at once.
#define LEADERBOARD_VISIBLE_ROWS 8

// Players report in twenty times a second, so this much silence means their
// client has stopped running frames. Their ship is not a live target.
#define PLAYER_STALE_SECONDS 2.0

// How long after a kill that player cannot be killed again. Without it a ship
// that is not running frames, and so never respawns, can be shot over and over.
#define KILL_COOLDOWN_SECONDS 3.0

// Levelling stops here. Reaching it takes long enough that the cap is a
// formality rather than something a session runs into.
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
    // Retired. One packet per round could not carry a thirty-round shotgun,
    // which is what FireVolley replaced it with.
    RetiredFireProjectile = 10,
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

    // Stamped by the server on the way out, ignored on the way in. This packet
    // already goes to everyone twenty times a second, so it costs nothing extra.
    float Health;
    float MaxHealth;
    uint8_t Level;

    // Which chassis to draw. Weapon branches need nothing here: they are already
    // plain to see in the shots themselves.
    uint8_t Evolution;
};

// 40 bytes per asteroid. No rotation matrix on purpose: the client works the spin
// out from Seed, which costs one byte rather than sixty-four.
struct AsteroidInfo
{
    // Stable for an asteroid's whole life and never reused, so a recycled pool
    // slot is never mistaken for the same asteroid having moved.
    uint32_t Id;
    uint8_t Seed;
    uint8_t Padding[3];
    Vector3 Position;
    Vector3 Velocity;
    float Scale;

    // What is left of it. Sent because a rock no longer dies to the first thing
    // that touches it.
    float Health;
};

// Only the first AsteroidCount entries are actually sent, so the packet varies
// in length. Use AsteroidPacketSize() rather than sizeof() when sending it.
struct AsteroidInfoPacket
{
    int Command;
    int AsteroidCount;
    AsteroidInfo Asteroids[MAX_ASTEROIDS];
};

// One round landing on one rock. The server reads the damage from the shooter's
// own build rather than from here.
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

// Sent by the shooter, who tests the shot exactly as it is drawn on their screen.
// Travels both ways: the server fills in KillerId and passes it to the victim.
struct PlayerKillPacket
{
    int Command;
    int KillerId;
    int VictimId;
};

// One whole trigger pull, however many rounds it becomes. The same weapon, place
// and volley number always give the same rounds, so only those have to travel.
struct VolleyPacket
{
    int Command;
    int PlayerID;

    // Where the ship was and which way it faced. Two axes rather than a matrix:
    // the third is their cross product.
    Vector3 Position;
    Vector3 Forward;
    Vector3 Up;

    // Stamped by the server from the shooter's build. A client says where it was
    // pointing, not how fast or how large its rounds are.
    float Speed;
    float Radius;
    float Lifetime;

    // Which weapon to lay the pattern out from, and which pull this is. The volley
    // number keeps an alternating pattern agreeing across machines.
    uint8_t WeaponId;
    uint8_t VolleyIndex;
    uint8_t Padding[2];
};

// Sent by the shooter. The damage is not in here: the server looks it up from
// the shooter's own build, so nobody can inflate what their guns do.
struct PlayerHitPacket
{
    int Command;
    int VictimId;
};

// Self-reported: we ran into a rock. Nobody gains by lying about being hurt, and
// the victim is the only one running the collision test against its own hull.
struct AsteroidCollisionPacket
{
    int Command;
    uint32_t AsteroidId;
};

// One player's progress and build, sent only to them and only when it changes.
// The offer travels with it because the server decides what is on the table.
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

// Our own hull. Position updates go to everyone except the player they describe,
// so a ship would otherwise learn every hull in the game except its own.
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

// Every packet starts with its command, so the type is read rather than guessed
// from the length. Callers still check the length separately.
inline int PeekCommand(const void* data, size_t length)
{
    if (length < sizeof(int))
        return -1;

    int command = 0;
    memcpy(&command, data, sizeof(int));
    return command;
}
