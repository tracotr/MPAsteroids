#pragma once

// Before NetClient.h, and so before raymath.h: raymath declares the vector types
// itself when raylib.h has not, and the two clash. Entity.h does the same.
#include "raylib/raylib.h"

#include "NetClient.h"

// The player's side of the upgrade system: the cards, the keys, and the two bars.
// Nothing here decides anything; the server rolls the offer and validates it.
namespace UpgradeUI
{
    // Keys 1, 2 and 3, live only while there is actually something to choose.
    void Update(NetClient& net);

    // Drawn over the world, after everything else in the HUD.
    void Draw(NetClient& net);
}
