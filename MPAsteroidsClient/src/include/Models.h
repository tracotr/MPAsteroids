#pragma once

#include "raylib/raylib.h"
#include "raylib/raymath.h"
#include "raylib/rlgl.h"

#include "networking/NetConstants.h"

#include <float.h>

namespace Models
{
    // Models used in game
    extern Model Skybox;
    extern Model ShipModel;
    extern BoundingBox ShipBoxLocal;
    extern Model AsteroidModel;
    extern BoundingBox AsteroidBoxLocal;
    
    // load models
    void Init();

    // draw models
    void DrawModel(Model model, const Vector3& position, const Matrix& rotation);
    void DrawSkybox();
    void DrawUI(Camera camera, Vector3 velocity, Vector3 position, int id, int (&scoreboard)[MAX_PLAYERS]);
    BoundingBox GetWorldBoundingBox(BoundingBox localBox, Vector3 position, Matrix rotation);
}