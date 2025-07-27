#pragma once

#include "raylib/raylib.h"
#include "raylib/raymath.h"
#include "raylib/rlgl.h"


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
    void Draw(Model model, const Vector3& position, const Matrix& rotation);
    void DrawSkybox();
    BoundingBox GetWorldBoundingBox(BoundingBox localBox, Vector3 position, Matrix rotation);
}