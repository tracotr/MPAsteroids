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

    void Update();
    void DrawModels();
    void CheckCollisions();
    void DrawUI(Camera camera);

    Player PlayerShip;
    BoundingBox AsteroidBoundingBoxes[MAX_ASTEROIDS];
};