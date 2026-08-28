#pragma once

#include "raylib/raylib.h"
#include "raylib/raymath.h"
#include "raylib/rlgl.h"

#include "networking/NetConstants.h"

#include <float.h>

namespace Models
{
    extern Model Skybox;
    extern Model ShipModel;
    extern BoundingBox ShipBoxLocal;
    extern Model AsteroidModel;
    extern BoundingBox AsteroidBoxLocal;

    // How far each model reaches from its centre, worked out once at load.
    // Collision uses these for a quick distance check before the full box test.
    extern float ShipRadiusLocal;
    extern float AsteroidRadiusLocal;

    // Average half-extent rather than the corner distance. A rock is treated as
    // a ball when it hits a ship, and the corner distance would make that ball
    // much fatter than the model actually looks.
    extern float AsteroidBodyRadius;

    
    void Init();

    void DrawModel(Model model, const Vector3& position, const Matrix& rotation, float scale = 1.0f);
    void DrawSkybox();
    void DrawUI(Camera camera, Vector3 velocity, Vector3 position, int id, int (&scoreboard)[MAX_PLAYERS], char (&names)[MAX_PLAYERS][MAX_PLAYER_NAME_LENGTH]);
    BoundingBox GetWorldBoundingBox(BoundingBox localBox, Vector3 position, Matrix rotation);
}