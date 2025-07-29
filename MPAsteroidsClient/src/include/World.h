#pragma once

#include "Entity.h"
#include "Player.h"
#include "networking/NetConstants.h"

class World
{
public:
    static World* Instance;

    static World& Create();
    static void Destroy();

    void Reset();

    void Update(double delta);
    void DrawModels();
    void DrawPlayerModels();
    void DrawAsteroidModels();
    void DrawShipLaser();
    void CreateAsteroidCollision();
    void CheckShipCollisions();
    void CheckLaserCollisions();
    void DrawUI(Camera camera);

    Player PlayerShip;
    BoundingBox AsteroidBoundingBoxes[MAX_ASTEROIDS];
};