#pragma once

#include "Entity.h"
#include "Player.h"
#include "Models.h"
#include "NetClient.h"
#include "Sounds.h"

#include "networking/NetConstants.h"

// How long a shot lives before it fades out.
const float PROJECTILE_LIFETIME = 2.0f;

// What a shot collides as before any upgrade widens it, and more forgiving than
// the dot that gets drawn.
const float PROJECTILE_HIT_RADIUS = 0.5f;

// How far a tracking round looks for something to steer toward.
const float PROJECTILE_HOMING_RANGE = 55.0f;

// How hard a tracking round can turn, in radians a second. Low enough that it
// bends toward a target rather than chasing one around the sky.
const float PROJECTILE_HOMING_TURN_RATE = 2.2f;

// Past this a round is not drawn. The wide weapons put too many in the air to
// spend anything on the ones nobody can see.
const float PROJECTILE_DRAW_DISTANCE = 120.0f;

// Past this, a player gets no name tag and no edge arrow, or the screen edges
// fill with markers for people nowhere near you.
const float PLAYER_INDICATOR_RANGE = 100.0f;

struct Projectile {
    bool active = false;

    // Who fired it, so a shot cannot hit the player who sent it. -1 means the
    // owner is unknown, which is treated as nobody's shot.
    int ownerId = -1;

    Vector3 position;

    // Where it was at the start of this frame. A shot can cross more ground in one
    // frame than a ship is wide, so collisions look along the whole step.
    Vector3 previousPosition;

    Vector3 velocity;
    float lifeTime;

    // Set from whoever fired it: our own shots from our build, everyone else's
    // from the size the server stamped on the packet.
    float radius = PROJECTILE_HIT_RADIUS;

    // How many more things this round can pass through before it stops.
    int pierceLeft = 0;

    // Steers toward whatever it is nearest. Set from the weapon that fired it, so
    // other people's rounds curve on our screen too.
    bool homing = false;

    // What this round does. Carried per round because a burst weapon can vary it:
    // a Double Dipper's second shot is half its first.
    float damage = BASE_DAMAGE;
};

// The most rounds waiting on a burst at once, across every ship in sight.
#define MAX_PENDING_ROUNDS 256

// A round from a burst weapon that has not left the barrel yet. The whole pull is
// laid out when the trigger goes down, and the later bursts wait here.
struct PendingRound {
    bool active = false;
    double dueTime = 0.0;

    Vector3 origin;
    Vector3 velocity;
    float radius = PROJECTILE_HIT_RADIUS;
    float lifeTime = PROJECTILE_LIFETIME;
    float damage = BASE_DAMAGE;
    int ownerId = -1;
    int pierce = 0;
    bool homing = false;
};

struct PlayerIndicator {
    Vector3 position;
    const char* name;

    // Server-stamped, so the bar is what that ship really has left. A negative max
    // means we have not heard yet and should draw nothing.
    float health;
    float maxHealth;
    int level;

    // Their weapon conceals who they are. The marker still shows, so they can be
    // seen and shot at; it is the name that is withheld.
    bool nameHidden;
};

// Everything the frame needs to know about one asteroid, gathered once per
// update so that rendering and both collision passes share the work.
struct AsteroidFrame {
    // The server's id. Entries get shuffled as asteroids are hit, so anything kept
    // past this frame has to use the id rather than the slot.
    uint32_t Id;
    Vector3 Position;
    Matrix Rotation;
    float Scale;

    // Reaches every corner, so it never misses a hit. Rejects far-off asteroids
    // before the real test runs.
    float Radius;

    // The rock's actual bulk, used for the test itself.
    float BodyRadius;
};

class World {
public:
    static World* Instance;
    static World& Create();
    static void Destroy();

    void Reset();
    void Update(double delta);
    void Draw();
    void DrawPlayerModels();

    void DrawAsteroidModels();
    void DrawUI();
    void DrawPlayerIndicators(const PlayerIndicator* otherPlayersData, int otherPlayerCount, int screenWidth, int screenHeight);

    void UpdateProjectiles(double delta);

    void FireProjectile(Vector3 position, Vector3 velocity, int ownerId, float lifeTime = PROJECTILE_LIFETIME,
                        float radius = PROJECTILE_HIT_RADIUS, int pierce = 0,
                        float damage = BASE_DAMAGE, bool homing = false);

    // One trigger pull, laid out in full by the shared pattern code. Anything not
    // in the first burst is queued rather than fired now.
    void FireVolley(Vector3 origin, const Matrix& rotation, const ShipStats& stats, int ownerId);

    // Releases queued burst rounds once their moment arrives.
    void UpdatePendingRounds(double now);

private:
    void ReleaseRound(Vector3 origin, Vector3 velocity, int ownerId, float lifeTime,
                      float radius, int pierce, float damage, bool homing);
    void QueueRound(double dueTime, Vector3 origin, Vector3 velocity, int ownerId,
                    float lifeTime, float radius, int pierce, float damage, bool homing);

    // Bends one tracking round toward the nearest thing worth hitting.
    void SteerHomingRound(Projectile& round, double delta);

    // Lays out a trigger pull that arrived from somebody else.
    void SpawnRemoteVolley(const NetClient::RemoteVolleyEvent& volley, float age);

public:

    void DrawProjectiles();
    void CheckCollisions();

    Projectile Projectiles[MAX_PROJECTILES];

private:
    void RefreshAsteroidFrame();
    bool CheckShipCollisions();
    bool CheckOutgoingFire();
    void CheckProjectileCollisions();

    AsteroidFrame AsteroidFrames[MAX_ASTEROIDS];
    int AsteroidFrameCount = 0;

    PendingRound PendingRounds[MAX_PENDING_ROUNDS];

    // One past the highest pool slot in use, so a quiet moment costs what a quiet
    // moment should rather than what a full pool would.
    int ProjectileHighWater = 0;

    // Counts our own trigger pulls. Alternating patterns read it, so it keeps
    // going up rather than resetting between weapons.
    int LocalVolleyIndex = 0;
};
