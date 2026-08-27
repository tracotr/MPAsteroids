#include "include/ServerHost.h"
#include "include/networking/NetConstants.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

namespace
{
    volatile std::sig_atomic_t g_shouldStop = 0;
    void HandleSignal(int) { g_shouldStop = 1; }
}

int main()
{
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    ServerHost server;
    if (!server.Start())
    {
        std::fprintf(stderr, "Failed to start server on port %d (is it already in use?)\n", SERVER_PORT);
        return 1;
    }

    std::printf("MPAsteroids server listening on ws://0.0.0.0:%d (max %d players)\n", SERVER_PORT, MAX_PLAYERS);
    std::printf("Press Ctrl+C to stop.\n");
    std::fflush(stdout);

    while (!g_shouldStop)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::printf("\nShutting down...\n");
    server.Stop();
    return 0;
}
