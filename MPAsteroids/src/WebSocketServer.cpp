#include "include/WebSocketServer.h"

#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>

#define WS_FD_SETSIZE 1024
#if defined(_WIN32)
    #define FD_SETSIZE WS_FD_SETSIZE
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET NativeSocket;
    static const NativeSocket kInvalidSocket = INVALID_SOCKET;
    static void CloseNativeSocket(NativeSocket s) { closesocket(s); }
    static void SetNonBlocking(NativeSocket s) { u_long mode = 1; ioctlsocket(s, FIONBIO, &mode); }
    static int LastSocketError() { return WSAGetLastError(); }
    #define WS_EWOULDBLOCK WSAEWOULDBLOCK
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int NativeSocket;
    static const NativeSocket kInvalidSocket = -1;
    static void CloseNativeSocket(NativeSocket s) { close(s); }
    static void SetNonBlocking(NativeSocket s) { int flags = fcntl(s, F_GETFL, 0); fcntl(s, F_SETFL, flags | O_NONBLOCK); }
    static int LastSocketError() { return errno; }
    #define WS_EWOULDBLOCK EWOULDBLOCK
#endif

namespace
{
    // ---- SHA1 (public-domain algorithm, written for this file) ----
    // Only used to derive the WebSocket handshake's Sec-WebSocket-Accept
    // value per RFC 6455 - not a security primitive, so a plain from-spec
    // implementation is appropriate here.
    struct Sha1Digest { uint8_t bytes[20]; };

    uint32_t RotateLeft(uint32_t value, int bits) { return (value << bits) | (value >> (32 - bits)); }

    Sha1Digest Sha1(const uint8_t* data, size_t len)
    {
        uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476, h4 = 0xC3D2E1F0;

        std::vector<uint8_t> msg(data, data + len);
        uint64_t bitLen = (uint64_t)len * 8;
        msg.push_back(0x80);
        while (msg.size() % 64 != 56) msg.push_back(0);
        for (int i = 7; i >= 0; --i) msg.push_back((uint8_t)(bitLen >> (i * 8)));

        for (size_t chunk = 0; chunk < msg.size(); chunk += 64)
        {
            uint32_t w[80];
            for (int i = 0; i < 16; ++i)
            {
                w[i] = (msg[chunk + i * 4] << 24) | (msg[chunk + i * 4 + 1] << 16) |
                       (msg[chunk + i * 4 + 2] << 8) | (msg[chunk + i * 4 + 3]);
            }
            for (int i = 16; i < 80; ++i)
                w[i] = RotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

            uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
            for (int i = 0; i < 80; ++i)
            {
                uint32_t f, k;
                if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999; }
                else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
                else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
                else { f = b ^ c ^ d; k = 0xCA62C1D6; }

                uint32_t temp = RotateLeft(a, 5) + f + e + k + w[i];
                e = d; d = c; c = RotateLeft(b, 30); b = a; a = temp;
            }

            h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
        }

        Sha1Digest digest;
        uint32_t h[5] = { h0, h1, h2, h3, h4 };
        for (int i = 0; i < 5; ++i)
        {
            digest.bytes[i * 4 + 0] = (uint8_t)(h[i] >> 24);
            digest.bytes[i * 4 + 1] = (uint8_t)(h[i] >> 16);
            digest.bytes[i * 4 + 2] = (uint8_t)(h[i] >> 8);
            digest.bytes[i * 4 + 3] = (uint8_t)(h[i]);
        }
        return digest;
    }

    std::string Base64Encode(const uint8_t* data, size_t len)
    {
        static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);

        for (size_t i = 0; i < len; i += 3)
        {
            uint32_t triple = (uint32_t)data[i] << 16;
            if (i + 1 < len) triple |= (uint32_t)data[i + 1] << 8;
            if (i + 2 < len) triple |= (uint32_t)data[i + 2];

            out += table[(triple >> 18) & 0x3F];
            out += table[(triple >> 12) & 0x3F];
            out += (i + 1 < len) ? table[(triple >> 6) & 0x3F] : '=';
            out += (i + 2 < len) ? table[triple & 0x3F] : '=';
        }
        return out;
    }

    std::string ComputeAcceptKey(const std::string& clientKey)
    {
        static const std::string magicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string combined = clientKey + magicGuid;
        Sha1Digest digest = Sha1(reinterpret_cast<const uint8_t*>(combined.data()), combined.size());
        return Base64Encode(digest.bytes, sizeof(digest.bytes));
    }

    // Case-insensitive search for a header value in a raw HTTP request.
    bool FindHeaderValue(const std::string& request, const char* headerName, std::string& outValue)
    {
        std::string lowerRequest = request;
        std::transform(lowerRequest.begin(), lowerRequest.end(), lowerRequest.begin(), ::tolower);
        std::string lowerName = headerName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        lowerName += ":";

        size_t pos = lowerRequest.find(lowerName);
        if (pos == std::string::npos) return false;

        size_t valueStart = pos + lowerName.size();
        while (valueStart < request.size() && (request[valueStart] == ' ' || request[valueStart] == '\t')) valueStart++;

        size_t lineEnd = request.find("\r\n", valueStart);
        if (lineEnd == std::string::npos) lineEnd = request.size();

        outValue = request.substr(valueStart, lineEnd - valueStart);
        return true;
    }
}

enum class ConnState { Handshaking, Open, ClosePending, Closed };

struct WebSocketServer::Connection
{
    NativeSocket socket = kInvalidSocket;
    WebSocketServer::ConnId id = -1;
    ConnState state = ConnState::Handshaking;
    std::vector<uint8_t> recvBuf;
    std::vector<uint8_t> sendBuf;
    bool announcedConnect = false;
};

WebSocketServer::WebSocketServer() : listenSocket(InvalidSocketValue())
{
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

WebSocketServer::~WebSocketServer()
{
    Stop();
#if defined(_WIN32)
    WSACleanup();
#endif
}

intptr_t WebSocketServer::InvalidSocketValue() { return (intptr_t)kInvalidSocket; }

bool WebSocketServer::Start(int port)
{
    NativeSocket sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == kInvalidSocket) return false;

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) != 0) { CloseNativeSocket(sock); return false; }
    if (listen(sock, 64) != 0) { CloseNativeSocket(sock); return false; }

    SetNonBlocking(sock);
    listenSocket = (intptr_t)sock;
    return true;
}

void WebSocketServer::Stop()
{
    for (Connection* conn : connections)
    {
        if (conn->socket != kInvalidSocket) CloseNativeSocket(conn->socket);
        delete conn;
    }
    connections.clear();

    if (listenSocket != InvalidSocketValue())
    {
        CloseNativeSocket((NativeSocket)listenSocket);
        listenSocket = InvalidSocketValue();
    }
}

namespace
{
    void AppendFrame(std::vector<uint8_t>& out, uint8_t opcode, const uint8_t* payload, size_t len)
    {
        out.push_back(0x80 | opcode); // FIN + opcode, server frames are never masked

        if (len < 126)
        {
            out.push_back((uint8_t)len);
        }
        else if (len <= 0xFFFF)
        {
            out.push_back(126);
            out.push_back((uint8_t)(len >> 8));
            out.push_back((uint8_t)(len));
        }
        else
        {
            out.push_back(127);
            for (int i = 7; i >= 0; --i) out.push_back((uint8_t)((uint64_t)len >> (i * 8)));
        }

        out.insert(out.end(), payload, payload + len);
    }
}

void WebSocketServer::Send(ConnId id, const void* data, size_t len)
{
    for (Connection* conn : connections)
    {
        if (conn->id == id && conn->state == ConnState::Open)
        {
            AppendFrame(conn->sendBuf, 0x2, (const uint8_t*)data, len);
            break;
        }
    }
}

void WebSocketServer::Disconnect(ConnId id)
{
    for (Connection* conn : connections)
    {
        if (conn->id == id && conn->state != ConnState::Closed)
        {
            uint8_t closeCode[2] = { 0x03, 0xE8 }; // 1000 Normal Closure
            AppendFrame(conn->sendBuf, 0x8, closeCode, sizeof(closeCode));
            conn->state = ConnState::ClosePending;
        }
    }
}

namespace
{
    // Tries to parse and consume one WebSocket frame from the front of
    // recvBuf. Returns true if a frame (possibly a control frame handled
    // internally) was consumed; sets outMessage/outClosed accordingly.
    bool TryParseFrame(std::vector<uint8_t>& recvBuf, std::vector<uint8_t>& sendBuf, std::vector<uint8_t>& outMessage, bool& gotMessage, bool& shouldClose)
    {
        gotMessage = false;
        shouldClose = false;

        if (recvBuf.size() < 2) return false;

        uint8_t byte0 = recvBuf[0];
        uint8_t byte1 = recvBuf[1];

        bool fin = (byte0 & 0x80) != 0;
        uint8_t opcode = byte0 & 0x0F;
        bool masked = (byte1 & 0x80) != 0;
        uint64_t payloadLen = byte1 & 0x7F;

        size_t headerSize = 2;
        if (payloadLen == 126)
        {
            if (recvBuf.size() < 4) return false;
            payloadLen = ((uint64_t)recvBuf[2] << 8) | recvBuf[3];
            headerSize = 4;
        }
        else if (payloadLen == 127)
        {
            if (recvBuf.size() < 10) return false;
            payloadLen = 0;
            for (int i = 0; i < 8; ++i) payloadLen = (payloadLen << 8) | recvBuf[2 + i];
            headerSize = 10;
        }

        // Guard against runaway allocations from a malformed/hostile frame header.
        const uint64_t kMaxPayload = 1 * 1024 * 1024;
        if (payloadLen > kMaxPayload) { shouldClose = true; return true; }

        size_t maskSize = masked ? 4 : 0;
        size_t totalSize = headerSize + maskSize + (size_t)payloadLen;
        if (recvBuf.size() < totalSize) return false;

        uint8_t maskKey[4] = { 0, 0, 0, 0 };
        if (masked) std::memcpy(maskKey, recvBuf.data() + headerSize, 4);

        const uint8_t* payloadStart = recvBuf.data() + headerSize + maskSize;

        if (!fin)
        {
            // Fragmented frames aren't supported - our protocol only ever
            // sends small, single-frame binary messages. Treat as fatal.
            shouldClose = true;
        }
        else if (opcode == 0x2 || opcode == 0x1) // binary or text
        {
            outMessage.resize((size_t)payloadLen);
            for (size_t i = 0; i < payloadLen; ++i)
                outMessage[i] = masked ? (payloadStart[i] ^ maskKey[i % 4]) : payloadStart[i];
            gotMessage = true;
        }
        else if (opcode == 0x8) // close
        {
            shouldClose = true;
        }
        else if (opcode == 0x9) // ping -> pong
        {
            std::vector<uint8_t> pongPayload((size_t)payloadLen);
            for (size_t i = 0; i < payloadLen; ++i)
                pongPayload[i] = masked ? (payloadStart[i] ^ maskKey[i % 4]) : payloadStart[i];
            AppendFrame(sendBuf, 0xA, pongPayload.data(), pongPayload.size());
        }
        // opcode 0xA (pong) and anything else: ignore

        recvBuf.erase(recvBuf.begin(), recvBuf.begin() + totalSize);
        return true;
    }
}

void WebSocketServer::Poll(int timeoutMs, const OnConnect& onConnect, const OnMessage& onMessage, const OnDisconnect& onDisconnect)
{
    if (listenSocket == InvalidSocketValue()) return;

    fd_set readSet, writeSet;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);

    NativeSocket listenSock = (NativeSocket)listenSocket;
    FD_SET(listenSock, &readSet);
    NativeSocket maxFd = listenSock;

    for (Connection* conn : connections)
    {
        if (conn->state == ConnState::Closed) continue;
        FD_SET(conn->socket, &readSet);
        if (!conn->sendBuf.empty()) FD_SET(conn->socket, &writeSet);
        if (conn->socket > maxFd) maxFd = conn->socket;
    }

    timeval tv = {};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int ready = select((int)maxFd + 1, &readSet, &writeSet, nullptr, &tv);
    if (ready < 0) return;

    // Accept new connections
    if (FD_ISSET(listenSock, &readSet))
    {
        for (;;)
        {
            NativeSocket clientSock = accept(listenSock, nullptr, nullptr);
            if (clientSock == kInvalidSocket) break;

            SetNonBlocking(clientSock);
            int noDelay = 1;
            setsockopt(clientSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&noDelay, sizeof(noDelay));

            Connection* conn = new Connection();
            conn->socket = clientSock;
            conn->id = nextConnId++;
            connections.push_back(conn);
        }
    }

    static uint8_t readChunk[8192];

    for (Connection* conn : connections)
    {
        if (conn->state == ConnState::Closed) continue;

        if (FD_ISSET(conn->socket, &readSet))
        {
            int n = recv(conn->socket, (char*)readChunk, sizeof(readChunk), 0);
            if (n > 0)
            {
                conn->recvBuf.insert(conn->recvBuf.end(), readChunk, readChunk + n);
            }
            else if (n == 0)
            {
                conn->state = ConnState::Closed;
            }
            else
            {
                int err = LastSocketError();
                if (err != WS_EWOULDBLOCK) conn->state = ConnState::Closed;
            }
        }

        if (conn->state == ConnState::Handshaking && !conn->recvBuf.empty())
        {
            std::string request(conn->recvBuf.begin(), conn->recvBuf.end());
            size_t headerEnd = request.find("\r\n\r\n");
            if (headerEnd != std::string::npos)
            {
                std::string clientKey;
                std::string response;
                if (FindHeaderValue(request, "Sec-WebSocket-Key", clientKey))
                {
                    std::string acceptKey = ComputeAcceptKey(clientKey);
                    response = "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\n"
                               "Connection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";
                    conn->sendBuf.insert(conn->sendBuf.end(), response.begin(), response.end());
                    conn->recvBuf.erase(conn->recvBuf.begin(), conn->recvBuf.begin() + (headerEnd + 4));
                    conn->state = ConnState::Open;
                }
                else
                {
                    conn->state = ConnState::Closed;
                }
            }
        }

        if (conn->state == ConnState::Open && !conn->announcedConnect)
        {
            conn->announcedConnect = true;
            if (onConnect) onConnect(conn->id);
        }

        if (conn->state == ConnState::Open)
        {
            std::vector<uint8_t> message;
            for (;;)
            {
                bool gotMessage = false;
                bool shouldClose = false;
                if (!TryParseFrame(conn->recvBuf, conn->sendBuf, message, gotMessage, shouldClose)) break;

                if (shouldClose) { conn->state = ConnState::Closed; break; }
                if (gotMessage && onMessage) onMessage(conn->id, message.data(), message.size());
            }
        }

        if (FD_ISSET(conn->socket, &writeSet) && !conn->sendBuf.empty())
        {
            int n = send(conn->socket, (const char*)conn->sendBuf.data(), (int)conn->sendBuf.size(), 0);
            if (n > 0)
            {
                conn->sendBuf.erase(conn->sendBuf.begin(), conn->sendBuf.begin() + n);
                if (conn->sendBuf.empty() && conn->state == ConnState::ClosePending)
                    conn->state = ConnState::Closed;
            }
            else if (n < 0)
            {
                int err = LastSocketError();
                if (err != WS_EWOULDBLOCK) conn->state = ConnState::Closed;
            }
        }
    }

    // Reap closed connections, notifying anyone who got a connect callback.
    for (size_t i = 0; i < connections.size();)
    {
        Connection* conn = connections[i];
        if (conn->state == ConnState::Closed)
        {
            if (conn->announcedConnect && onDisconnect) onDisconnect(conn->id);
            CloseNativeSocket(conn->socket);
            delete conn;
            connections.erase(connections.begin() + i);
        }
        else
        {
            ++i;
        }
    }
}
