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

    virtual void Update(double delta) override;
    void Draw() override;

    void Reset();
    void Respawn();

    Vector3 GetForwardVector();
};