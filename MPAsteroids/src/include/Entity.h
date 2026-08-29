#pragma once

#include "raylib/raylib.h"
#include "raylib/raymath.h"

class Entity
{
public:
    // Top speed and acceleration live in ShipStats, because upgrades move them.
    // Turning is still per frame, since nothing upgrades it.
    float RotationSpeed = 0.025f;
    float SlowRotationSpeed = 0.01f;

    Vector3 Position = { 0.0f, 0.0f, 0.0f };
    Matrix Rotation = MatrixIdentity();
    Vector3 Velocity = { 0.0f, 0.0f, 0.0f };

    const Vector3 Forward = (Vector3){ 0.0f, 0.0f, -1.0f };
    const Vector3 Backward = (Vector3){ 0.0f, 0.0f, 1.0f };

    virtual void Update(double delta);
    virtual void Draw();
};