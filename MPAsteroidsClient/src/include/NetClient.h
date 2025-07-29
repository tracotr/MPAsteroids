#pragma once

#include "raylib/raymath.h"
#include "networking/NetConstants.h"

void NetConnect(const char* address);

void UpdateLocalPlayer(Vector3 pos, Matrix rot);
int GetLocalPlayerId();
bool GetPlayerSpatial(int id, Vector3* pos, Matrix* Rotation);
void HandleAddAsteroid(AsteroidInfoPacket packet);
void HandleUpdateAsteroid(AsteroidInfoPacket packet);
void HandleDestroyAsteroid(int playerIdx, int asteroidIdx);
bool GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot);
int GetMaxAsteroids();

void NetUpdate(double now, float delta);