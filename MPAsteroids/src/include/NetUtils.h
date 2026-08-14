#pragma once
#include <string>

namespace NetUtils
{
    // Resolves and returns the local LAN IPv4 address.
    // Returns "127.0.0.1" on failure.
    std::string GetLocalIPv4();
}