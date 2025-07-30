#pragma once
#include "net_common.h"
#include "PlayerInfo.h"
#include <random>

class AsteroidManager
{
private:
    AsteroidInfo Asteroids[MAX_ASTEROIDS] = { 0 };
    int AsteroidAmount = 0;

    AsteroidInfo CreateAsteroid();
    void WrapAsteroid(AsteroidInfo* asteroid, Vector3 center, float wrapDistance);
    Vector3 GetNearestPlayerPosition(PlayerInfo (&players)[MAX_PLAYERS], Vector3 asteroidPos);
    float RandBetween(float min, float max);

public:
   
    
    double lastAsteroidUpdateTime = 0;
    const double ASTEROID_TICK_INTERVAL = 1.0f / 30.0f; 

    void UpdateAsteroids(PlayerInfo (&players)[MAX_PLAYERS], double delta);
    bool GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot);
    void RespawnAsteroid(int id);
    void SpawnAsteroids(int amount);
    AsteroidInfo (&GetAsteroids())[MAX_ASTEROIDS] { return Asteroids; };
    int GetAsteroidAmount() { return AsteroidAmount; };
};