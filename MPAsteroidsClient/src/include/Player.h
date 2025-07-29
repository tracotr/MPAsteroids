#pragma once

#include "Entity.h"

class Player : public Entity
{
public:
    double laserCooldown = 0.0;
    const double LASER_COOLDOWN_DURATION = 0.75;
    bool isFiring = false;

    // need to update and draw player
    void Update(double delta) override;
    void Draw() override;

    // functions to reset player
    void Reset();
    void Respawn();

    // functions to shoot laser
    Vector3 GetForwardVector();
};