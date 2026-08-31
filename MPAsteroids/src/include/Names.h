#pragma once

#include <string>

// Random player names, read from resources/names.txt.
namespace Names
{
    // Reads the word lists. Call once at startup, alongside the other loaders.
    void Init();

    // An adjective, a noun, and two digits, e.g. "SillyOtter42".
    std::string MakeRandom();
}
