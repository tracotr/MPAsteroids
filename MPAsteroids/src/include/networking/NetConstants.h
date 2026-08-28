#pragma once

#include "../raylib/raymath.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#define MAX_PLAYERS 64

// Most asteroids the server will ever hold at once. The usual number is far
// lower; the spare room is there so a burst of splits always has space.
#define MAX_ASTEROIDS 256

// The play area is a set of cubes this wide. Each player switches on the block
// of eight that meets at the lattice corner nearest them, so they are always
// well inside the world rather than standing on its edge. Go somewhere new and
// more cubes switch on, so the world grows as people explore.
#define REGION_CELL_SIZE 35.0f

// Cells switch off again after this long with nobody inside, which keeps the
// world roughly the size of the space actually being used.
#define REGION_CELL_TTL 12.0

// Ceiling on how many cubes can be switched on at once.
#define MAX_REGION_CELLS 64

// Asteroids per cube, so the field stays as dense when the world is large as it
// is when everyone is together. Set the total ceiling low enough to keep the
// broadcast small.
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

// Players appear roughly this far out from the centre, spread around it,
// instead of all stacked on the same spot in the middle. Wide enough that
// respawning does not drop anyone back into the fight they just lost: a shot
// covers thirty units a second, so distance here buys time.
//
// Going further just switches on more of the world, so there is no need to keep
// this inside any particular boundary.
#define PLAYER_SPAWN_RADIUS 50.0f

#define MAX_SQR_V3 3.402823466e+38F
// Includes the terminator, so names are this minus one character. Sized to fit
// the longest adjective-and-noun pair in resources/names.txt with room to spare.
// It is a field in PlayerPacket and ScoreboardPacket, so changing it changes the
// wire format: server and client have to be rebuilt together.
#define MAX_PLAYER_NAME_LENGTH 32

// The smallest piece worth making. An asteroid shatters instead of splitting
// once shrinking it by ASTEROID_SPLIT_FACTOR would take it below this.
#define MIN_ASTEROID_SCALE 0.45f
#define ASTEROID_SPLIT_FACTOR 0.68f

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

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

// Players report their position twenty times a second, so this much silence
// means their client has stopped running frames: a backgrounded browser tab, or
// a connection that has dropped without the socket noticing yet. Their ship is
// left behind at its last position, and must not be treated as a live target.
#define PLAYER_STALE_SECONDS 2.0

// How long after a kill that player cannot be killed again. Without it a ship
// that is not running frames, and so never respawns, can be shot over and over.
#define KILL_COOLDOWN_SECONDS 3.0

enum NetworkCommands
{
    AcceptPlayer = 1,
    AddPlayer = 2,
    RemovePlayer = 3,
    UpdatePlayer = 4,
    UpdateInput = 5,
    UpdateAsteroid = 6,
    DestroyAsteroid = 7,
    UpdateScoreboard = 8,
    ResetScoreboardId = 9,
    FireProjectile = 10,
    PlayerKilled = 11,
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
};

// 36 bytes per asteroid in each packet. There is no rotation matrix on purpose:
// the client works the spin out from Seed instead, which costs one byte rather
// than sixty-four and still has every player see the same rock turning.
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
};

// Only the first AsteroidCount entries are actually sent, so the packet varies
// in length. Use AsteroidPacketSize() rather than sizeof() when sending it.
struct AsteroidInfoPacket
{
    int Command;
    int AsteroidCount;
    AsteroidInfo Asteroids[MAX_ASTEROIDS];
};

struct AsteroidDestroyPacket
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

// Sent by the shooter, who tests the shot against the target exactly as it is
// drawn on their screen, so a hit lands when it looks like it should. The victim
// cannot judge this itself: a backgrounded browser tab stops rendering, and a
// client that is not running frames is not running collision either.
//
// Travels both ways. The shooter fills in VictimId; the server fills in KillerId
// from the connection and passes it on to the victim, who then respawns.
struct PlayerKillPacket
{
    int Command;
    int KillerId;
    int VictimId;
};

struct ProjectilePacket
{
    int Command;
    int PlayerID;
    Vector3 Position;
    Vector3 Velocity;
};
#pragma pack(pop)

static_assert(ASTEROID_MAX_TOTAL <= MAX_ASTEROIDS, "asteroid field must fit the pool");
static_assert(sizeof(AsteroidInfo) == 36, "AsteroidInfo is a wire format; update the size noted above");

inline size_t AsteroidPacketSize(int count)
{
    return offsetof(AsteroidInfoPacket, Asteroids) + (size_t)count * sizeof(AsteroidInfo);
}

// Every packet starts with its command, so the type can be read straight off
// instead of being guessed from the length. Callers still check the length
// separately, against whatever that command should be carrying.
inline int PeekCommand(const void* data, size_t length)
{
    if (length < sizeof(int))
        return -1;

    int command = 0;
    memcpy(&command, data, sizeof(int));
    return command;
}
