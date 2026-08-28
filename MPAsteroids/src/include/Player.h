#pragma once

#include "Entity.h"

class Player : public Entity
{
public:
    double laserCooldown = 0.0;
    const float LASER_SPEED = 30.0f;
    const double LASER_COOLDOWN_DURATION = 0.25;

    // Radians of turn per pixel of mouse movement.
    const float MOUSE_SENSITIVITY = 0.0025f;

    // Matches what the shift key does to the keyboard turn rate.
    const float MOUSE_FINE_FACTOR = 0.25f;

    bool isFiring = false;
    bool boosterActive = false;

    virtual void Update(double delta) override;
    void Draw() override;

    void Reset();
    void Respawn();

    Vector3 GetForwardVector();
};