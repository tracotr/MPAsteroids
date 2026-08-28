#pragma once

#include <string>

// Random player names, read from resources/names.txt rather than baked into the
// code so the word lists can be edited without touching C++.
namespace Names
{
    // Reads the word lists. Call once at startup, alongside the other loaders.
    void Init();

    // An adjective, a noun, and two digits, e.g. "SillyOtter42".
    std::string MakeRandom();
}
