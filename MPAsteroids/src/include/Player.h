#pragma once

#include "Entity.h"

class Player : public Entity
{
public:
    double laserCooldown = 0.0;
    const float LASER_SPEED = 30.0f;
    const double LASER_COOLDOWN_DURATION = 0.25;
    bool isFiring = false;
    bool boosterActive = false;

    // update and draw player
    virtual void Update(double delta) override;
    void Draw() override;

    // functions to reset player
    void Reset();
    void Respawn();

    // functions to shoot laser
    Vector3 GetForwardVector();
};