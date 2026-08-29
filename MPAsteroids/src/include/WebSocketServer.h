#pragma once

// Minimal WebSocket server: binary messages, no extensions, no TLS, since a proxy
// terminates wss:// for us. Single-threaded, sized for a lobby of dozens.

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

    // Services accepts, handshakes, reads and writes, blocking at most timeoutMs
    // and invoking the callbacks synchronously before it returns.
    void Poll(int timeoutMs, const OnConnect& onConnect, const OnMessage& onMessage, const OnDisconnect& onDisconnect);

    // Queued, not sent immediately; the next Poll() flushes it.
    void Send(ConnId id, const void* data, size_t len);
    void Disconnect(ConnId id);

private:
    struct Connection;

    static intptr_t InvalidSocketValue();

    std::vector<Connection*> connections;
    intptr_t listenSocket;
    int nextConnId = 1;
};
