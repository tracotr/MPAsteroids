#pragma once

#include "Entity.h"
#include "Player.h"
#include "Models.h"
#include "NetClient.h"

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

    NetClient NetClient;
};