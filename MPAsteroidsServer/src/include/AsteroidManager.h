#pragma once
#include "networking/NetCommon.h"
#include "raylib/raymath.h"
#include "PlayerInfo.h"
#include "NetworkUtil.h"
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
    const double ASTEROID_TICK_INTERVAL = 1.0f / 20.0f; 

    void UpdateAsteroids(PlayerInfo (&players)[MAX_PLAYERS], double delta);
    void RespawnAsteroid(int id);
    void SpawnAsteroids(int amount);
    AsteroidInfo (&GetAsteroids())[MAX_ASTEROIDS] { return Asteroids; };
    int GetAsteroidAmount() { return AsteroidAmount; };
};