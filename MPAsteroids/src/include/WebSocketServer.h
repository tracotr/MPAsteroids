#pragma once

// Minimal RFC 6455 WebSocket server (binary messages only, no TLS, no
// extensions). TLS is intentionally out of scope here: the dedicated server
// is meant to run behind a reverse proxy (nginx/Caddy) that terminates
// wss:// and forwards plain ws:// traffic to this server locally.
//
// Single-threaded, select()-based, sized for dozens of concurrent
// connections (a game lobby), not internet-facing high-concurrency loads.

#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>

class WebSocketServer
{
public:
    using ConnId = int;

    using OnConnect = std::function<void(ConnId)>;
    using OnMessage = std::function<void(ConnId, const uint8_t* data, size_t len)>;
    using OnDisconnect = std::function<void(ConnId)>;

    WebSocketServer();
    ~WebSocketServer();

    bool Start(int port);
    void Stop();
    bool IsRunning() const { return listenSocket != InvalidSocketValue(); }

    // Non-blocking: services accepts, handshakes, reads and writes for up to
    // timeoutMs, invoking the given callbacks synchronously for whatever
    // happened during this call. Safe to call every server tick.
    void Poll(int timeoutMs, const OnConnect& onConnect, const OnMessage& onMessage, const OnDisconnect& onDisconnect);

    // Queues a binary message to a connection; flushed on the next Poll().
    void Send(ConnId id, const void* data, size_t len);
    void Disconnect(ConnId id);

private:
    struct Connection;

    static intptr_t InvalidSocketValue();

    std::vector<Connection*> connections;
    intptr_t listenSocket;
    int nextConnId = 1;
};
