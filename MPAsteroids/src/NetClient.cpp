#include "include/NetClient.h"

#include <cstring>
#include <cstdio>

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

namespace
{
    EMSCRIPTEN_WEBSOCKET_T g_socket = 0;
    NetClient* g_activeClient = nullptr;

    void CopySafeName(char* destination, size_t destinationSize, const char* source)
    {
        if (destination == nullptr || destinationSize == 0)
            return;

        std::memset(destination, 0, destinationSize);
        if (source == nullptr)
            return;

        std::strncpy(destination, source, destinationSize - 1);
        destination[destinationSize - 1] = '\0';
    }

    EM_BOOL OnOpen(int, const EmscriptenWebSocketOpenEvent*, void*)
    {
        if (g_activeClient) g_activeClient->OnSocketOpen();
        return EM_TRUE;
    }

    EM_BOOL OnClose(int, const EmscriptenWebSocketCloseEvent*, void*)
    {
        if (g_activeClient) g_activeClient->OnSocketClosed();
        return EM_TRUE;
    }

    EM_BOOL OnError(int, const EmscriptenWebSocketErrorEvent*, void*)
    {
        if (g_activeClient) g_activeClient->OnSocketClosed();
        return EM_TRUE;
    }

    EM_BOOL OnMessage(int, const EmscriptenWebSocketMessageEvent* event, void*)
    {
        if (g_activeClient && event->isText == EM_FALSE)
            g_activeClient->OnSocketMessage(event->data, event->numBytes);
        return EM_TRUE;
    }
}

bool NetClient::NetConnect(const char* serverAddress, const char* playerName)
{
    NetDisconnect();

    LocalPlayerId = -1;
    RemoteProjectileCount = 0;
    AsteroidAmount = 0;
    std::memset(Players, 0, sizeof(Players));
    std::memset(PlayerNames, 0, sizeof(PlayerNames));
    std::memset(Scoreboard, 0, sizeof(Scoreboard));

    CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), playerName);
    if (LocalPlayerName[0] == '\0')
        CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), "Player");

    if (!emscripten_websocket_is_supported())
    {
        printf("[CLIENT] WebSockets are not supported in this browser\n");
        return false;
    }

    // Browsers block ws:// from an https:// page, so follow the page's scheme.
    const bool pageIsSecure = EM_ASM_INT({ return location.protocol === 'https:' ? 1 : 0; }) != 0;
    const char* scheme = pageIsSecure ? "wss" : "ws";
    const int port = SERVER_PUBLIC_PORT;
    const bool portIsDefault = (pageIsSecure && port == 443) || (!pageIsSecure && port == 80);

    char url[256];
    if (portIsDefault)
        std::snprintf(url, sizeof(url), "%s://%s%s", scheme, serverAddress, SERVER_PATH);
    else
        std::snprintf(url, sizeof(url), "%s://%s:%d%s", scheme, serverAddress, port, SERVER_PATH);

    // The compile-time address is only a default. The page can retarget this build
    // without a rebuild, which matters when the server sits behind a tunnel whose
    // hostname changes. Accepts a bare host, host:port, or a full URL.
    char serverOverride[256] = { 0 };

    // EM_ASM splits its body on top-level commas, so declare each var separately.
    EM_ASM({
        var out = $0;
        var max = $1;
        var value = "";
        try {
            var fromQuery = new URLSearchParams(location.search).get("server");
            if (fromQuery) { value = fromQuery; }
            else if (typeof window.MPASTEROIDS_SERVER === "string") { value = window.MPASTEROIDS_SERVER; }
        } catch (e) { value = ""; }
        stringToUTF8(String(value), out, max);
    }, serverOverride, (int)sizeof(serverOverride));

    if (serverOverride[0] != '\0')
    {
        if (std::strstr(serverOverride, "://") != nullptr)
            std::snprintf(url, sizeof(url), "%s", serverOverride);
        else if (std::strchr(serverOverride, '/') != nullptr)
            std::snprintf(url, sizeof(url), "%s://%s", scheme, serverOverride);
        else
            std::snprintf(url, sizeof(url), "%s://%s%s", scheme, serverOverride, SERVER_PATH);

        printf("[CLIENT] Server override: %s\n", url);
    }

    EmscriptenWebSocketCreateAttributes attributes = {};
    attributes.url = url;
    attributes.protocols = nullptr;
    attributes.createOnMainThread = EM_TRUE;

    g_socket = emscripten_websocket_new(&attributes);
    if (g_socket <= 0)
    {
        printf("[CLIENT] Failed to create WebSocket for %s\n", url);
        g_socket = 0;
        return false;
    }

    g_activeClient = this;
    Status = NetStatus::Connecting;

    emscripten_websocket_set_onopen_callback(g_socket, nullptr, OnOpen);
    emscripten_websocket_set_onclose_callback(g_socket, nullptr, OnClose);
    emscripten_websocket_set_onerror_callback(g_socket, nullptr, OnError);
    emscripten_websocket_set_onmessage_callback(g_socket, nullptr, OnMessage);

    return true;
}

void NetClient::NetDisconnect()
{
    if (g_socket > 0)
    {
        emscripten_websocket_close(g_socket, 1000, "client shutdown");
        emscripten_websocket_delete(g_socket);
        g_socket = 0;
    }

    g_activeClient = nullptr;
    Status = NetStatus::Disconnected;
    LocalPlayerId = -1;
}

void NetClient::SendPacket(const void* data, size_t length)
{
    if (g_socket <= 0 || Status != NetStatus::Connected)
        return;

    emscripten_websocket_send_binary(g_socket, const_cast<void*>(data), (uint32_t)length);
}

void NetClient::OnSocketOpen()
{
    Status = NetStatus::Connected;
    LastInputSend = -UPDATE_INTERVAL;

    PlayerPacket announce = {};
    announce.Command = static_cast<int>(NetworkCommands::UpdateInput);
    CopySafeName(announce.Name, sizeof(announce.Name), LocalPlayerName);
    announce.Position = (Vector3){ 0.0f, 0.0f, 0.0f };
    announce.Rotation = MatrixIdentity();

    SendPacket(&announce, sizeof(announce));
}

void NetClient::OnSocketClosed()
{
    Status = NetStatus::Disconnected;
    LocalPlayerId = -1;
    AsteroidAmount = 0;
    RemoteProjectileCount = 0;
    std::memset(Players, 0, sizeof(Players));
}

void NetClient::OnSocketMessage(const uint8_t* data, size_t length)
{
    DispatchPacket(data, length);
}

void NetClient::HandleAddPlayer(PlayerPacket packet)
{
	int remotePlayer = packet.Id;

	if(remotePlayer < 0 || remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId)
		return;

	Players[remotePlayer].Active = true;
    CopySafeName(Players[remotePlayer].Name, sizeof(Players[remotePlayer].Name), packet.Name);
    CopySafeName(PlayerNames[remotePlayer], sizeof(PlayerNames[remotePlayer]), packet.Name);

	Players[remotePlayer].Position = packet.Position;
    Players[remotePlayer].TargetPosition = packet.Position;
	Players[remotePlayer].Rotation = packet.Rotation;
	Players[remotePlayer].LastUpdateTime = LastNow;
}

void NetClient::HandleRemovePlayer(PlayerPacket packet)
{
	int remotePlayer = packet.Id;

	if(remotePlayer < 0 || remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId)
		return;

	Players[remotePlayer].Active = false;
    Players[remotePlayer].Name[0] = '\0';
    PlayerNames[remotePlayer][0] = '\0';
}

void NetClient::HandleUpdatePlayer(PlayerPacket packet)
{
	int remotePlayer = packet.Id;

	if(remotePlayer < 0 || remotePlayer >= MAX_PLAYERS || remotePlayer == LocalPlayerId || !Players[remotePlayer].Active)
		return;

    CopySafeName(Players[remotePlayer].Name, sizeof(Players[remotePlayer].Name), packet.Name);
    CopySafeName(PlayerNames[remotePlayer], sizeof(PlayerNames[remotePlayer]), packet.Name);

	Players[remotePlayer].TargetPosition = packet.Position;
    Players[remotePlayer].Rotation = packet.Rotation;
	Players[remotePlayer].LastUpdateTime = LastNow;
}

void NetClient::UpdateLocalPlayer(Vector3 pos, Matrix rot)
{
    // The server hasn't assigned us a slot yet.
	if(LocalPlayerId < 0)
        return;

    Players[LocalPlayerId].Position = pos;
    Players[LocalPlayerId].Rotation = rot;
}

bool NetClient::GetPlayerSpatial(int id, Vector3* pos, Matrix* rot)
{
    // Callers draw the result, and the local ship is drawn separately.
    if (id < 0 || id >= MAX_PLAYERS || !Players[id].Active || id == LocalPlayerId)
    {
        return false;
    }

    *pos = Players[id].Position;
    *rot = Players[id].Rotation;
    return true;
}

void NetClient::HandlePlayerCollision()
{
    ScoreboardPacket scoreboardBuffer = {};
    scoreboardBuffer.Command = NetworkCommands::ResetScoreboardId;
    scoreboardBuffer.Id = LocalPlayerId;

    SendPacket(&scoreboardBuffer, sizeof(scoreboardBuffer));
}

void NetClient::HandleUpdateAsteroid(AsteroidInfoPacket packet)
{
    for(int i = 0; i < packet.AsteroidCount && i < MAX_ASTEROIDS; i++) {
        Asteroids[i] = packet.AllAsteroids[i];
    }
    AsteroidAmount = packet.AsteroidCount;
}

void NetClient::HandleDestroyAsteroid(int playerIdx, int asteroidIdx)
{
    AsteroidDestroyPacket buffer = {};
    buffer.Command = NetworkCommands::DestroyAsteroid;
    buffer.PlayerID = playerIdx;
    buffer.AsteroidID = asteroidIdx;

    SendPacket(&buffer, sizeof(buffer));
}

bool NetClient::GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot, float* scale)
{
    if(id < 0 || id >= AsteroidAmount)
    {
        return false;
    }

    *pos = Asteroids[id].Position;
    *rot = Asteroids[id].Rotation;
    if (scale != nullptr)
        *scale = Asteroids[id].Scale;
    return true;
}

void NetClient::HandleUpdateScoreboard(ScoreboardPacket packet)
{
    memcpy(Scoreboard, packet.Scoreboard, sizeof(packet.Scoreboard));
    for (int i = 0; i < MAX_PLAYERS; ++i)
        CopySafeName(PlayerNames[i], sizeof(PlayerNames[i]), packet.Names[i]);
}

// Packet type is identified by payload size; every packet struct has a distinct one.
void NetClient::DispatchPacket(const uint8_t* data, size_t length)
{
    if (length < 1)
        return;

    if (length == sizeof(PlayerPacket))
    {
        PlayerPacket recieved;
        memcpy(&recieved, data, sizeof(PlayerPacket));

        // Until the server assigns us an id, AcceptPlayer is the only packet we act on.
        if (LocalPlayerId != -1)
        {
            switch (recieved.Command)
            {
                case NetworkCommands::AddPlayer:
                    HandleAddPlayer(recieved);
                    break;

                case NetworkCommands::RemovePlayer:
                    HandleRemovePlayer(recieved);
                    break;

                case NetworkCommands::UpdatePlayer:
                    HandleUpdatePlayer(recieved);
                    break;
            }
        }
        else
        {
            if (recieved.Command != NetworkCommands::AcceptPlayer)
                return;

            if (recieved.Id < 0 || recieved.Id >= MAX_PLAYERS)
                return;

            LocalPlayerId = recieved.Id;

            if (recieved.Name[0] != '\0')
                CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), recieved.Name);
            else if (LocalPlayerName[0] == '\0')
                CopySafeName(LocalPlayerName, sizeof(LocalPlayerName), "Player");
            CopySafeName(PlayerNames[LocalPlayerId], sizeof(PlayerNames[LocalPlayerId]), LocalPlayerName);

            LastInputSend = -UPDATE_INTERVAL;

            Players[LocalPlayerId].Active = true;
            CopySafeName(Players[LocalPlayerId].Name, sizeof(Players[LocalPlayerId].Name), LocalPlayerName);
            Players[LocalPlayerId].Position = (Vector3){ 0.0f, 0.0f, 0.0f };
        }
    }
    else if (length == sizeof(AsteroidInfoPacket))
    {
        AsteroidInfoPacket recieved;
        memcpy(&recieved, data, sizeof(AsteroidInfoPacket));

        if (recieved.Command == NetworkCommands::UpdateAsteroid)
            HandleUpdateAsteroid(recieved);
    }
    else if (length == sizeof(ScoreboardPacket))
    {
        ScoreboardPacket recieved;
        memcpy(&recieved, data, sizeof(ScoreboardPacket));

        if (recieved.Command == NetworkCommands::UpdateScoreboard)
            HandleUpdateScoreboard(recieved);
    }
    else if (length == sizeof(ProjectilePacket))
    {
        ProjectilePacket recieved;
        memcpy(&recieved, data, sizeof(ProjectilePacket));

        if (recieved.Command == NetworkCommands::FireProjectile)
        {
            if (RemoteProjectileCount < MAX_PROJECTILES)
            {
                RemoteProjectilesQueue[RemoteProjectileCount].Position = recieved.Position;
                RemoteProjectilesQueue[RemoteProjectileCount].Velocity = recieved.Velocity;
                RemoteProjectileCount++;
            }
        }
    }
}

void NetClient::NetUpdate(double now, float delta)
{
    LastNow = now;

    if (Status != NetStatus::Connected)
        return;

    // The server only broadcasts state a few times a second, so both asteroids and
    // remote players are advanced locally between updates to keep motion smooth:
    // asteroids extrapolate along their last known velocity, players ease toward
    // the last position received.
    for (int i = 0; i < AsteroidAmount; i++)
    {
        Asteroids[i].Position.x += Asteroids[i].Velocity.x * delta;
        Asteroids[i].Position.y += Asteroids[i].Velocity.y * delta;
        Asteroids[i].Position.z += Asteroids[i].Velocity.z * delta;
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (i != LocalPlayerId && Players[i].Active)
        {
            float lerpSpeed = 10.0f; // Higher = snappier, lower = floatier.
            Players[i].Position.x += (Players[i].TargetPosition.x - Players[i].Position.x) * lerpSpeed * delta;
            Players[i].Position.y += (Players[i].TargetPosition.y - Players[i].Position.y) * lerpSpeed * delta;
            Players[i].Position.z += (Players[i].TargetPosition.z - Players[i].Position.z) * lerpSpeed * delta;
        }
    }

    if(LocalPlayerId >= 0 && now - LastInputSend > UPDATE_INTERVAL)
    {
        PlayerPacket buffer = {};
        buffer.Command = NetworkCommands::UpdateInput;
        CopySafeName(buffer.Name, sizeof(buffer.Name), LocalPlayerName);
        buffer.Position = Players[LocalPlayerId].Position;
        buffer.Rotation = Players[LocalPlayerId].Rotation;

        SendPacket(&buffer, sizeof(buffer));

        LastInputSend = now;
    }
}

void NetClient::SendProjectile(Vector3 position, Vector3 velocity)
{
    ProjectilePacket buffer = {};
    buffer.Command = NetworkCommands::FireProjectile;
    buffer.PlayerID = LocalPlayerId;
    buffer.Position = position;
    buffer.Velocity = velocity;

    SendPacket(&buffer, sizeof(buffer));
}
