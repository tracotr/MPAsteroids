#pragma once

// Before NetClient.h, and so before raymath.h.
#include "raylib/raylib.h"

#include "NetClient.h"

// The player's side of the upgrade system: the cards, the keys, and the two bars.
namespace UpgradeUI
{
    // Keys 1, 2 and 3, live only while there is actually something to choose.
    void Update(NetClient& net);

    // Drawn over the world, after everything else in the HUD.
    void Draw(NetClient& net);
}
