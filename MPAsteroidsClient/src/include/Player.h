#pragma once

#include "Entity.h"

class Player : public Entity
{
public:
    // need to update and draw player
    void Update() override;
    void Draw() const override;

    // functions to reset player
    void Reset();
    void Respawn();
};