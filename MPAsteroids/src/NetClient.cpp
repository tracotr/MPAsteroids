#include "include/NetClient.h"

#include <cstring>
#include <cstdio>
#include <cmath>

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
    AsteroidCount = 0;
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
    AsteroidCount = 0;
    RemoteProjectileCount = 0;
    killedPending = false;
    killedBy = -1;
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

// Whether a player has gone quiet. Deliberately not folded into GetPlayerSpatial:
// hiding a ship for this makes anyone on a throttled or briefly stuttering
// connection blink in and out. They stay drawn, they just cannot be shot, which
// is the only part that ever mattered.
// A clock that keeps running while the tab is in the background, unlike the
// frame time, which is what makes it usable for measuring queue delay.
static double NowSeconds()
{
    return emscripten_get_now() * 0.001;
}

double NetClient::QueuedProjectileAge(int index) const
{
    if (index < 0 || index >= RemoteProjectileCount)
        return 0.0;

    double age = NowSeconds() - RemoteProjectilesQueue[index].ArrivalTime;
    return age > 0.0 ? age : 0.0;
}

bool NetClient::IsPlayerStale(int id) const
{
    if (id < 0 || id >= MAX_PLAYERS || !Players[id].Active)
        return true;

    return (LastNow - Players[id].LastUpdateTime) > PLAYER_STALE_SECONDS;
}

void NetClient::HandlePlayerCollision()
{
    ScoreboardPacket scoreboardBuffer = {};
    scoreboardBuffer.Command = NetworkCommands::ResetScoreboardId;
    scoreboardBuffer.Id = LocalPlayerId;

    SendPacket(&scoreboardBuffer, sizeof(scoreboardBuffer));
}

namespace
{
    // Asteroids arrive with a seed instead of a rotation, so the spin is worked
    // out here. It comes from the seed alone, so every player sees the same rock
    // turning the same way without any of it being sent.
    void DeriveSpin(uint8_t seed, Vector3* axis, float* speed)
    {
        uint32_t hash = (uint32_t)seed * 2654435761u;
        hash ^= hash >> 13;

        float azimuth = (float)((hash >> 8) & 1023u) / 1023.0f * 2.0f * PI;
        float height = (float)((hash >> 18) & 1023u) / 1023.0f * 2.0f - 1.0f;
        float ring = sqrtf(fmaxf(0.0f, 1.0f - height * height));

        *axis = (Vector3){ ring * cosf(azimuth), ring * sinf(azimuth), height };
        *speed = 0.15f + (float)(hash & 255u) / 255.0f * 0.55f;
    }
}

// Merges an update into the list we already hold, in place. Asteroids we have
// seen before keep the position we were drawing them at and how far through
// their spin they are; ones the server no longer lists are dropped. Matched by
// Id and never by array slot, since both sides reuse slots freely.
void NetClient::ReportKill(int victimId)
{
    PlayerKillPacket buffer = {};
    buffer.Command = NetworkCommands::PlayerKilled;
    buffer.KillerId = LocalPlayerId;
    buffer.VictimId = victimId;

    SendPacket(&buffer, sizeof(buffer));
}

bool NetClient::ConsumeKilled(int* killerId)
{
    if (!killedPending)
        return false;

    if (killerId != nullptr) *killerId = killedBy;
    killedPending = false;
    killedBy = -1;
    return true;
}

void NetClient::ApplyAsteroidSnapshot(const AsteroidInfoPacket& packet, int count)
{
    bool stillPresent[MAX_ASTEROIDS] = { false };

    for (int i = 0; i < count; i++)
    {
        const AsteroidInfo& info = packet.Asteroids[i];

        int slot = -1;
        for (int j = 0; j < AsteroidCount; j++)
        {
            if (Asteroids[j].Id == info.Id)
            {
                slot = j;
                break;
            }
        }

        bool isNew = (slot == -1);
        if (isNew)
        {
            if (AsteroidCount >= MAX_ASTEROIDS)
                continue;

            slot = AsteroidCount++;
            Asteroids[slot] = ClientAsteroid{};
            Asteroids[slot].Id = info.Id;
            Asteroids[slot].Position = info.Position;
            DeriveSpin(info.Seed, &Asteroids[slot].SpinAxis, &Asteroids[slot].SpinSpeed);
            Asteroids[slot].SpinAngle = (float)(info.Seed) * 0.0245f;
        }

        ClientAsteroid& asteroid = Asteroids[slot];
        asteroid.ServerPosition = info.Position;
        asteroid.Velocity = info.Velocity;
        asteroid.Scale = info.Scale;

        // Too far off to be our own guess drifting: the server has moved it.
        if (Vector3DistanceSqr(asteroid.Position, asteroid.ServerPosition) >
            ASTEROID_SNAP_DISTANCE * ASTEROID_SNAP_DISTANCE)
        {
            asteroid.Position = asteroid.ServerPosition;
        }

        // The server disagreed that this one died, so stop hiding it.
        if (asteroid.DestroyReportedAt > 0.0 && LastNow - asteroid.DestroyReportedAt >= DESTROY_REPORT_GRACE)
            asteroid.DestroyReportedAt = -1.0;

        stillPresent[slot] = true;
    }

    // Drop everything the server no longer lists, filling each gap with the
    // last live entry so the array stays packed.
    for (int i = AsteroidCount - 1; i >= 0; i--)
    {
        if (stillPresent[i])
            continue;

        AsteroidCount--;
        if (i != AsteroidCount)
        {
            Asteroids[i] = Asteroids[AsteroidCount];
            stillPresent[i] = stillPresent[AsteroidCount];
        }
    }
}

void NetClient::ReportAsteroidDestroyed(uint32_t asteroidId)
{
    if (asteroidId == 0)
        return;

    for (int i = 0; i < AsteroidCount; i++)
    {
        if (Asteroids[i].Id != asteroidId)
            continue;

        // Already reported and still within the wait period, so sending it
        // again would just be ignored by the server.
        if (Asteroids[i].DestroyReportedAt > 0.0 && LastNow - Asteroids[i].DestroyReportedAt < DESTROY_REPORT_GRACE)
            return;

        // Hidden without waiting for the round trip, so shooting feels
        // immediate. The grace period restores it if the server declines.
        Asteroids[i].DestroyReportedAt = LastNow;
        break;
    }

    AsteroidDestroyPacket buffer = {};
    buffer.Command = NetworkCommands::DestroyAsteroid;
    buffer.PlayerID = LocalPlayerId;
    buffer.AsteroidId = asteroidId;

    SendPacket(&buffer, sizeof(buffer));
}

uint32_t NetClient::GetAsteroidId(int index) const
{
    if (index < 0 || index >= AsteroidCount)
        return 0;

    return Asteroids[index].Id;
}

bool NetClient::GetAsteroidSpatial(int index, Vector3* pos, Matrix* rot, float* scale)
{
    if (index < 0 || index >= AsteroidCount)
        return false;

    const ClientAsteroid& asteroid = Asteroids[index];

    // Reported destroyed locally and not yet confirmed: treat it as gone, so it
    // is neither drawn nor collided with.
    if (asteroid.DestroyReportedAt > 0.0 && LastNow - asteroid.DestroyReportedAt < DESTROY_REPORT_GRACE)
        return false;

    *pos = asteroid.Position;
    *rot = MatrixRotate(asteroid.SpinAxis, asteroid.SpinAngle);
    if (scale != nullptr)
        *scale = asteroid.Scale;
    return true;
}

void NetClient::HandleUpdateScoreboard(ScoreboardPacket packet)
{
    memcpy(Scoreboard, packet.Scoreboard, sizeof(packet.Scoreboard));
    for (int i = 0; i < MAX_PLAYERS; ++i)
        CopySafeName(PlayerNames[i], sizeof(PlayerNames[i]), packet.Names[i]);
}

// Dispatched on the leading command field; see PeekCommand.
void NetClient::DispatchPacket(const uint8_t* data, size_t length)
{
    const int command = PeekCommand(data, length);
    if (command < 0)
        return;

    if (command == NetworkCommands::AcceptPlayer || command == NetworkCommands::AddPlayer ||
        command == NetworkCommands::RemovePlayer || command == NetworkCommands::UpdatePlayer)
    {
        if (length != sizeof(PlayerPacket))
            return;

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
    else if (command == NetworkCommands::UpdateAsteroid)
    {
        if (length < AsteroidPacketSize(0))
            return;

        int count = 0;
        memcpy(&count, data + offsetof(AsteroidInfoPacket, AsteroidCount), sizeof(int));
        if (count < 0 || count > MAX_ASTEROIDS || length != AsteroidPacketSize(count))
            return;

        AsteroidInfoPacket recieved;
        memcpy(&recieved, data, AsteroidPacketSize(count));
        ApplyAsteroidSnapshot(recieved, count);
    }
    else if (command == NetworkCommands::PlayerKilled)
    {
        if (length != sizeof(PlayerKillPacket))
            return;

        PlayerKillPacket recieved;
        memcpy(&recieved, data, sizeof(PlayerKillPacket));

        if (recieved.VictimId == LocalPlayerId)
        {
            killedPending = true;
            killedBy = recieved.KillerId;
        }
    }
    else if (command == NetworkCommands::UpdateScoreboard)
    {
        if (length != sizeof(ScoreboardPacket))
            return;

        ScoreboardPacket recieved;
        memcpy(&recieved, data, sizeof(ScoreboardPacket));
        HandleUpdateScoreboard(recieved);
    }
    else if (command == NetworkCommands::FireProjectile)
    {
        if (length != sizeof(ProjectilePacket))
            return;

        ProjectilePacket recieved;
        memcpy(&recieved, data, sizeof(ProjectilePacket));

        if (RemoteProjectileCount < MAX_PROJECTILES)
        {
            RemoteProjectilesQueue[RemoteProjectileCount].ArrivalTime = NowSeconds();
            RemoteProjectilesQueue[RemoteProjectileCount].PlayerId = recieved.PlayerID;
            RemoteProjectilesQueue[RemoteProjectileCount].Position = recieved.Position;
            RemoteProjectilesQueue[RemoteProjectileCount].Velocity = recieved.Velocity;
            RemoteProjectileCount++;
        }
    }
}

void NetClient::NetUpdate(double now, float delta)
{
    LastNow = now;

    if (Status != NetStatus::Connected)
        return;

    // The server only sends updates ten times a second, so we keep asteroids and
    // other players moving ourselves in between. When an update arrives we slide
    // toward it rather than snapping to it; snapping throws away the movement we
    // just did and makes the whole field twitch every time a packet lands.
    //
    // The amounts below use exp() rather than "rate * delta" so they work out the
    // same at 30fps and 240fps, and cannot overshoot on a slow frame.
    const float asteroidBlend = 1.0f - expf(-ASTEROID_CORRECTION_RATE * delta);
    const float playerBlend = 1.0f - expf(-PLAYER_CORRECTION_RATE * delta);

    for (int i = 0; i < AsteroidCount; i++)
    {
        ClientAsteroid& asteroid = Asteroids[i];

        Vector3 step = Vector3Scale(asteroid.Velocity, delta);
        asteroid.ServerPosition = Vector3Add(asteroid.ServerPosition, step);
        asteroid.Position = Vector3Add(asteroid.Position, step);
        asteroid.Position = Vector3Lerp(asteroid.Position, asteroid.ServerPosition, asteroidBlend);

        asteroid.SpinAngle += asteroid.SpinSpeed * delta;
        if (asteroid.SpinAngle > 2.0f * PI)
            asteroid.SpinAngle -= 2.0f * PI;
    }

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (i != LocalPlayerId && Players[i].Active)
            Players[i].Position = Vector3Lerp(Players[i].Position, Players[i].TargetPosition, playerBlend);
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
