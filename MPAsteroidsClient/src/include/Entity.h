#pragma once

#include "raylib/raylib.h"
#include "raylib/raymath.h"

class Entity
{
public:
    bool Alive = false;

    // movement speeds
    float BaseSpeed = 0.5f;
    float MaxSpeed = 55.0f;
    float RotationSpeed = 0.05f;

    // Spatial vectors
    Vector3 Position = { 0.0f, 0.0f, 0.0f };
    Matrix Rotation = MatrixIdentity();
    Vector3 Velocity = { 0.0f, 0.0f, 0.0f };

    // Directional vectors
    const Vector3 Forward = (Vector3){ 0.0f, 0.0f, -1.0f };
    const Vector3 Backward = (Vector3){ 0.0f, 0.0f, 1.0f };

    const Vector3 Up = (Vector3){ 1.0f, 0.0f, 0.0f };
    const Vector3 Down = (Vector3){ -1.0f, 0.0f, 0.0f };
    const Vector3 Left = (Vector3){ 0.0f, 1.0f, 0.0f };
    const Vector3 Right = (Vector3){ 0.0f, -1.0f, 0.0f };

    virtual void Update(double delta);
    virtual void Draw();
};