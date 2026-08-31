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
    RemoteVolleyCount = 0;
    AsteroidCount = 0;

    // The server sends the real build the moment it accepts us.
    Upgrades.Reset();
    LocalHealth = Upgrades.Stats().MaxHealth;
    LocalMaxHealth = Upgrades.Stats().MaxHealth;
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

    // The compile-time address is only a default; the page can retarget this build without a rebuild.
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
    RemoteVolleyCount = 0;
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

    Players[remotePlayer].Health = packet.Health;
    Players[remotePlayer].MaxHealth = packet.MaxHealth;
    Players[remotePlayer].Level = packet.Level;
    Players[remotePlayer].Evolution = packet.Evolution;
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

    Players[remotePlayer].Health = packet.Health;
    Players[remotePlayer].MaxHealth = packet.MaxHealth;
    Players[remotePlayer].Level = packet.Level;
    Players[remotePlayer].Evolution = packet.Evolution;
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

// Whether a player has gone quiet.
static double NowSeconds()
{
    return emscripten_get_now() * 0.001;
}

double NetClient::QueuedVolleyAge(int index) const
{
    if (index < 0 || index >= RemoteVolleyCount)
        return 0.0;

    double age = NowSeconds() - RemoteVolleyQueue[index].ArrivalTime;
    return age > 0.0 ? age : 0.0;
}

bool NetClient::IsPlayerStale(int id) const
{
    if (id < 0 || id >= MAX_PLAYERS || !Players[id].Active)
        return true;

    return (LastNow - Players[id].LastUpdateTime) > PLAYER_STALE_SECONDS;
}

// The rock is hidden now, because it is about to break whatever else happens.
void NetClient::ReportAsteroidCollision(uint32_t asteroidId)
{
    if (asteroidId == 0)
        return;

    HideAsteroidLocally(asteroidId);

    AsteroidCollisionPacket buffer = {};
    buffer.Command = NetworkCommands::AsteroidCollision;
    buffer.AsteroidId = asteroidId;

    SendPacket(&buffer, sizeof(buffer));
}

void NetClient::SendUpgradeChoice(uint8_t upgradeId)
{
    UpgradeChoosePacket buffer = {};
    buffer.Command = NetworkCommands::ChooseUpgrade;
    buffer.UpgradeId = upgradeId;

    SendPacket(&buffer, sizeof(buffer));
}

bool NetClient::GetPlayerHealth(int id, float* health, float* maxHealth) const
{
    if (id < 0 || id >= MAX_PLAYERS || !Players[id].Active || Players[id].MaxHealth <= 0.0f)
        return false;

    if (health != nullptr) *health = Players[id].Health;
    if (maxHealth != nullptr) *maxHealth = Players[id].MaxHealth;
    return true;
}

uint8_t NetClient::GetPlayerEvolution(int id) const
{
    if (id < 0 || id >= MAX_PLAYERS || !Players[id].Active)
        return UPGRADE_NONE;

    return Players[id].Evolution;
}

int NetClient::GetPlayerLevel(int id) const
{
    if (id < 0 || id >= MAX_PLAYERS || !Players[id].Active)
        return 0;

    return (int)Players[id].Level;
}

namespace
{
    // Asteroids arrive with a seed instead of a rotation, so the spin is worked out here.
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

// Merges an update into the list we hold, keeping the position and spin of rocks we have seen before.
void NetClient::ReportHit(int victimId)
{
    PlayerHitPacket buffer = {};
    buffer.Command = NetworkCommands::PlayerHit;
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

        // The server's figure wins. Ours was a guess made between broadcasts.
        asteroid.Health = info.Health;

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

    // Drop everything the server no longer lists.
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

// Returns false when the same asteroid was hidden a moment ago.
void NetClient::HideAsteroidLocally(uint32_t asteroidId)
{
    for (int i = 0; i < AsteroidCount; i++)
    {
        if (Asteroids[i].Id != asteroidId)
            continue;

        // Hidden without waiting for the server to answer, so hitting a rock feels immediate.
        Asteroids[i].DestroyReportedAt = LastNow;
        return;
    }
}

bool NetClient::ReportAsteroidHit(uint32_t asteroidId, float damage)
{
    if (asteroidId == 0)
        return false;

    bool finished = false;

    for (int i = 0; i < AsteroidCount; i++)
    {
        if (Asteroids[i].Id != asteroidId)
            continue;

        // Already hidden and still within the wait period: our guess says this rock is gone.
        if (Asteroids[i].DestroyReportedAt > 0.0 && LastNow - Asteroids[i].DestroyReportedAt < DESTROY_REPORT_GRACE)
            return true;

        Asteroids[i].Health -= damage;

        // Hidden without waiting for the reply, so the laser that finishes a rock feels like it did.
        if (Asteroids[i].Health <= 0.0f)
        {
            Asteroids[i].DestroyReportedAt = LastNow;
            finished = true;
        }
        break;
    }

    AsteroidHitPacket buffer = {};
    buffer.Command = NetworkCommands::HitAsteroid;
    buffer.PlayerID = LocalPlayerId;
    buffer.AsteroidId = asteroidId;

    SendPacket(&buffer, sizeof(buffer));
    return finished;
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

    // Reported destroyed locally and not yet confirmed: treat it as gone.
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
    else if (command == NetworkCommands::FireVolley)
    {
        if (length != sizeof(VolleyPacket))
            return;

        VolleyPacket recieved;
        memcpy(&recieved, data, sizeof(VolleyPacket));

        if (RemoteVolleyCount < MAX_REMOTE_VOLLEYS)
        {
            RemoteVolleyEvent& queued = RemoteVolleyQueue[RemoteVolleyCount++];
            queued.ArrivalTime = NowSeconds();
            queued.PlayerId = recieved.PlayerID;
            queued.Position = recieved.Position;
            queued.Forward = recieved.Forward;
            queued.Up = recieved.Up;
            queued.Speed = recieved.Speed;
            queued.Radius = recieved.Radius;
            queued.Lifetime = recieved.Lifetime;
            queued.WeaponId = recieved.WeaponId;
            queued.VolleyIndex = recieved.VolleyIndex;
        }
    }
    else if (command == NetworkCommands::UpdateUpgrades)
    {
        if (length != sizeof(UpgradeStatePacket))
            return;

        UpgradeStatePacket recieved;
        memcpy(&recieved, data, sizeof(UpgradeStatePacket));

        // Everything the ship can do is recomputed from this by the same catalog the server ran.
        Upgrades.ReadFrom(recieved);
    }
    else if (command == NetworkCommands::UpdateHealth)
    {
        if (length != sizeof(PlayerHealthPacket))
            return;

        PlayerHealthPacket recieved;
        memcpy(&recieved, data, sizeof(PlayerHealthPacket));

        LocalHealth = recieved.Health;
        LocalMaxHealth = recieved.MaxHealth;
    }
}

void NetClient::NetUpdate(double now, float delta)
{
    LastNow = now;

    if (Status != NetStatus::Connected)
        return;

    // Updates come ten times a second, so we keep things moving in between and
    // slide toward each one rather than snapping, which would make the field twitch.
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

void NetClient::SendVolley(Vector3 position, Vector3 forward, Vector3 up, int volleyIndex)
{
    VolleyPacket buffer = {};
    buffer.Command = NetworkCommands::FireVolley;
    buffer.PlayerID = LocalPlayerId;
    buffer.Position = position;
    buffer.Forward = forward;
    buffer.Up = up;
    buffer.VolleyIndex = (uint8_t)(volleyIndex & 0xFF);

    // Speed, size, range and weapon are all stamped by the server from our build before it passes this on.
    SendPacket(&buffer, sizeof(buffer));
}
