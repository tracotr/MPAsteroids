#pragma once

#include "raylib/raymath.h"
#include "networking/NetConstants.h"

void NetConnect(const char* address);
void NetUpdate(double now, float delta);
void UpdateLocalPlayer(Vector3 pos, Matrix rot);
int GetLocalPlayerId();
bool GetPlayerSpatial(int id, Vector3* pos, Matrix* Rotation);
bool GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot);
int GetMaxAsteroids();