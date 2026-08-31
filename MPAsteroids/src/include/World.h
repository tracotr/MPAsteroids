#pragma once

#include "Entity.h"
#include "Player.h"
#include "Models.h"
#include "NetClient.h"
#include "Sounds.h"

#include "networking/NetConstants.h"

// How long a laser lives before it fades out.
const float LASER_LIFETIME = 2.0f;

// What a laser collides as before any upgrade widens it, and more forgiving than the dot that gets drawn.
const float LASER_HIT_RADIUS = 0.5f;

// How far a tracking laser looks for something to steer toward.
const float LASER_HOMING_RANGE = 55.0f;

// How hard a tracking laser can turn, in radians a second.
const float LASER_HOMING_TURN_RATE = 2.2f;

// Past this a laser is not drawn.
const float LASER_DRAW_DISTANCE = 120.0f;

// Past this, a player gets no name tag and no edge arrow.
const float PLAYER_INDICATOR_RANGE = 150.0f;

struct Laser {
    bool active = false;

    // Who fired it, so a laser cannot hit the player who sent it. -1 means the owner is unknown.
    int ownerId = -1;

    Vector3 position;

    // Where it was at the start of this frame.
    Vector3 previousPosition;

    Vector3 velocity;
    float lifeTime;

    // Set from whoever fired it: our own lasers from our build.
    float radius = LASER_HIT_RADIUS;

    // How many more things this laser can pass through before it stops.
    int pierceLeft = 0;

    // Steers toward whatever it is nearest.
    bool homing = false;

    // What this laser does. Carried per laser because a burst weapon can vary it.
    float damage = BASE_DAMAGE;
};

// The most lasers waiting on a burst at once, across every ship in sight.
#define MAX_PENDING_LASERS 256

// A laser from a burst weapon that has not left the barrel yet.
struct PendingLaser {
    bool active = false;
    double dueTime = 0.0;

    Vector3 origin;
    Vector3 velocity;
    float radius = LASER_HIT_RADIUS;
    float lifeTime = LASER_LIFETIME;
    float damage = BASE_DAMAGE;
    int ownerId = -1;
    int pierce = 0;
    bool homing = false;
};

struct PlayerIndicator {
    Vector3 position;
    const char* name;

    // Server-stamped, so the bar is what that ship really has left.
    float health;
    float maxHealth;
    int level;

    // Their weapon conceals who they are.
    bool nameHidden;
};

// Everything the frame needs to know about one asteroid, gathered once per update.
struct AsteroidFrame {
    // The server's id. Entries get shuffled as asteroids are hit.
    uint32_t Id;
    Vector3 Position;
    Matrix Rotation;
    float Scale;

    // Reaches every corner, so it never misses a hit.
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
    void DrawWorldBoundary(Vector3 viewer);
    void DrawPlayerModels();

    void DrawAsteroidModels();
    void DrawUI();
    void DrawPlayerIndicators(const PlayerIndicator* otherPlayersData, int otherPlayerCount, int screenWidth, int screenHeight);

    void UpdateLasers(double delta);

    void FireLaser(Vector3 position, Vector3 velocity, int ownerId, float lifeTime = LASER_LIFETIME,
                        float radius = LASER_HIT_RADIUS, int pierce = 0,
                        float damage = BASE_DAMAGE, bool homing = false);

    // One trigger pull, laid out in full by the shared pattern code.
    void FireVolley(Vector3 origin, const Matrix& rotation, const ShipStats& stats, int ownerId);

    // Releases queued burst lasers once their moment arrives.
    void UpdatePendingLasers(double now);

private:
    void ReleaseLaser(Vector3 origin, Vector3 velocity, int ownerId, float lifeTime,
                      float radius, int pierce, float damage, bool homing);
    void QueueLaser(double dueTime, Vector3 origin, Vector3 velocity, int ownerId,
                    float lifeTime, float radius, int pierce, float damage, bool homing);

    // Bends one tracking laser toward the nearest thing worth hitting.
    void SteerHomingLaser(Laser& laser, double delta);

    // Lays out a trigger pull that arrived from somebody else.
    void SpawnRemoteVolley(const NetClient::RemoteVolleyEvent& volley, float age);

public:

    void DrawLasers();
    void CheckCollisions();

    Laser Lasers[MAX_LASERS];

private:
    void RefreshAsteroidFrame();
    bool CheckShipCollisions();
    bool CheckOutgoingFire();
    void CheckLaserCollisions();

    AsteroidFrame AsteroidFrames[MAX_ASTEROIDS];
    int AsteroidFrameCount = 0;

    PendingLaser PendingLasers[MAX_PENDING_LASERS];

    // One past the highest pool slot in use.
    int LaserHighWater = 0;

    // Counts our own trigger pulls.
    int LocalVolleyIndex = 0;
};
