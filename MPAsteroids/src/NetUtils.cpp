// Windows Socket definitions
#ifndef _WIN32
#define _WINSOCKAPI_
#endif
#define NOUSER
#define NOGDI
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>

#include "include/NetUtils.h"

namespace NetUtils
{
    std::string GetLocalIPv4()
    {
        WSADATA wsaData = {};
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            return "127.0.0.1";

        char hostname[256] = { 0 };
        if (gethostname(hostname, sizeof(hostname)) != 0)
        {
            WSACleanup();
            return "127.0.0.1";
        }

        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo* results = nullptr;
        if (getaddrinfo(hostname, nullptr, &hints, &results) != 0)
        {
            WSACleanup();
            return "127.0.0.1";
        }

        std::string resolved = "127.0.0.1";
        for (addrinfo* current = results; current != nullptr; current = current->ai_next)
        {
            sockaddr_in* sockaddr = reinterpret_cast<sockaddr_in*>(current->ai_addr);
            if (sockaddr == nullptr)
                continue;

            char ip[INET_ADDRSTRLEN] = { 0 };
            inet_ntop(AF_INET, &sockaddr->sin_addr, ip, sizeof(ip));

            if (std::strcmp(ip, "0.0.0.0") == 0 || std::strcmp(ip, "127.0.0.1") == 0)
                continue;

            resolved = ip;
            break;
        }

        freeaddrinfo(results);
        WSACleanup();
        return resolved;
    }
}