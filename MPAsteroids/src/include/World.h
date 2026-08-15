#pragma once

#include "Entity.h"
#include "Player.h"
#include "Models.h"
#include "NetClient.h"
#include "Sounds.h"

#include "networking/NetConstants.h"

#include <utility>
#include <string>
#include <vector>

class World
{
public:
    static World* Instance;

    static World& Create();
    static void Destroy();

    void Reset();

    void Update(double delta, Camera3D camera);
    void Draw();
    void DrawPlayerModels();
    
    void DrawAsteroidModels();
    void DrawShipLaser();
    void DrawUI(Camera camera);
    void DrawPlayerIndicators(Camera3D camera, const std::vector<std::pair<Vector3, std::string>>& otherPlayersData, int screenWidth, int screenHeight);
    void CreateAsteroidCollision();
    void CheckShipCollisions(BoundingBox asteroidBox, Camera3D camera);
    void CheckLaserCollisions(BoundingBox asteroidBox, int asteroidId, Camera3D camera);
    void CheckCollisions(Camera3D camera);

    Player PlayerShip;
    BoundingBox AsteroidBoundingBoxes[MAX_ASTEROIDS];

    NetClient Net;
};