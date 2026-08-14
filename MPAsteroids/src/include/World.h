#pragma once

#include "Entity.h"
#include "Player.h"
#include "Models.h"
#include "NetClient.h"
#include "Sounds.h"

#include "networking/NetConstants.h"

class World
{
public:
    static World* Instance;

    static World& Create();
    static void Destroy();

    void Reset();

    void Update(double delta);
    void Draw();
    void DrawPlayerModels();
    void DrawAsteroidModels();
    void DrawShipLaser();
    void DrawUI(Camera camera);
    void CreateAsteroidCollision();
    void CheckShipCollisions(BoundingBox asteroidBox);
    void CheckLaserCollisions(BoundingBox asteroidBox, int asteroidId);
    void CheckCollisions();

    Player PlayerShip;
    BoundingBox AsteroidBoundingBoxes[MAX_ASTEROIDS];

    // Named "Net" rather than "NetClient" to avoid shadowing the NetClient
    // type name itself (newer GCC treats that as a hard error, not just a
    // warning, which broke builds on toolchains other than the one this
    // project was originally pinned to).
    NetClient Net;
};