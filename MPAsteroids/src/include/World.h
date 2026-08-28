#pragma once

#include "Entity.h"
#include "Player.h"
#include "Models.h"
#include "NetClient.h"
#include "Sounds.h"

#include "networking/NetConstants.h"

// How long a shot lives before it fades out.
const float PROJECTILE_LIFETIME = 2.0f;

// Shots are treated as small spheres for collision, a little more forgiving
// than the dot that gets drawn.
const float PROJECTILE_HIT_RADIUS = 0.5f;

// Past this, a player gets no name tag and no edge arrow. The world is open
// enough now that without a limit the screen edges fill with markers for people
// nowhere near you.
const float PLAYER_INDICATOR_RANGE = 100.0f;

struct Projectile {
    bool active = false;

    // Who fired it, so a shot cannot hit the player who sent it. -1 means the
    // owner is unknown, which is treated as nobody's shot.
    int ownerId = -1;

    Vector3 position;

    // Where it was at the start of this frame. A shot can cross more ground in
    // one frame than a ship is wide, so collisions look along the whole step
    // rather than only at where it ended up.
    Vector3 previousPosition;

    Vector3 velocity;
    float lifeTime;
};

struct PlayerIndicator {
    Vector3 position;
    const char* name;
};

// Everything the frame needs to know about one asteroid, gathered once per
// update so that rendering and both collision passes share the work.
struct AsteroidFrame {
    // The server's id for this asteroid. Entries get shuffled around as
    // asteroids are hit, so anything outside this frame has to use the id
    // rather than the position in the list.
    uint32_t Id;
    Vector3 Position;
    Matrix Rotation;
    float Scale;

    // Reaches every corner, so it never misses a hit. Used to reject far-off
    // asteroids before doing the real test.
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

    void FireProjectile(Vector3 position, Vector3 velocity, int ownerId, float lifeTime = PROJECTILE_LIFETIME);

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
};
