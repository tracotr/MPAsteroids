#pragma once

#include "Entity.h"
#include "Player.h"

class World
{
public:
    static World* Instance;

    static World& Create();
    static void Destroy();

    void Reset();

    void Update();
    void DrawModels();
    void DrawUI(Camera camera);

    Player PlayerShip;
};