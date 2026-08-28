#include "include/ServerHost.h"

#include "include/WebSocketServer.h"
#include "include/networking/NetConstants.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <random>
#include <string>
#include <thread>

#define BASE_SCORE 5

const double SERVER_TICK_RATE = 30.0;
const double SERVER_TICK_INTERVAL = 1.0 / SERVER_TICK_RATE;

struct ServerPlayer
{
    bool Active = false;
    bool ValidPosition = false;
    WebSocketServer::ConnId ConnId = -1;
    char Name[MAX_PLAYER_NAME_LENGTH] = { 0 };
    Vector3 Position = { 0.0f, 0.0f, 0.0f };
    Matrix Rotation = MatrixIdentity();
    int Score = 0;
};

class ServerHost::Impl
{
public:
    Impl() : running(false) {}
    ~Impl() { Stop(); }

    bool Start()
    {
        if (running.load())
            return true;

        if (!wsServer.Start(SERVER_PORT))
            return false;

        running.store(true);
        worker = std::thread(&Impl::Run, this);
        return true;
    }

    void Stop()
    {
        if (!running.load())
            return;

        running.store(false);
        if (worker.joinable())
            worker.join();

        wsServer.Stop();
    }

    bool IsRunning() const { return running.load(); }
    std::string GetHostAddress() const { return "127.0.0.1"; }

private:
    std::atomic<bool> running;
    std::thread worker;
    WebSocketServer wsServer;
    ServerPlayer players[MAX_PLAYERS] = {};
    AsteroidInfo asteroids[MAX_ASTEROIDS] = {};
    int asteroidAmount = 0;

    double asteroidHitCooldown[MAX_ASTEROIDS] = { 0.0 };

    static double GetClockSeconds()
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    float RandBetween(float min, float max)
    {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(min, max);
        return dist(gen);
    }

    AsteroidInfo CreateAsteroid()
    {
        AsteroidInfo asteroid = {};
        asteroid.Position = (Vector3){ RandBetween(-25.0f, 25.0f), RandBetween(-25.0f, 25.0f), RandBetween(-25.0f, 25.0f) };
        asteroid.Velocity = (Vector3){ RandBetween(-2.0f, 2.0f), RandBetween(-2.0f, 2.0f), RandBetween(-2.0f, 2.0f) };
        asteroid.Rotation = MatrixIdentity();
        asteroid.Scale = 1.0f;
        asteroid.Active = true;
        return asteroid;
    }

    void SpawnAsteroids(int amount)
    {
        for (int i = 0; i < amount && asteroidAmount < MAX_ASTEROIDS; ++i)
            asteroids[asteroidAmount++] = CreateAsteroid();
    }

    bool BreakAsteroid(int id)
    {
        if (id < 0 || id >= asteroidAmount)
            return false;

        double now = GetClockSeconds();
        if (now - asteroidHitCooldown[id] < 0.1)
            return false;

        asteroidHitCooldown[id] = now;

        AsteroidInfo& asteroid = asteroids[id];

        if (asteroid.Scale <= MIN_ASTEROID_SCALE)
        {
            asteroids[id] = CreateAsteroid();
            return true;
        }

        float splitScale = asteroid.Scale * 0.7f;

        Vector3 tangent = Vector3Normalize((Vector3){ asteroid.Velocity.z, asteroid.Velocity.y, -asteroid.Velocity.x });

        if (Vector3LengthSqr(tangent) < 0.0001f)
            tangent = (Vector3){ 1.0f, 0.0f, 0.0f };

        Vector3 splitOffset = Vector3Scale(tangent, asteroid.Scale * 1.1f);

        AsteroidInfo leftAsteroid = asteroid;
        AsteroidInfo rightAsteroid = asteroid;

        leftAsteroid.Scale = splitScale;
        rightAsteroid.Scale = splitScale;

        leftAsteroid.Position = Vector3Add(asteroid.Position, splitOffset);
        rightAsteroid.Position = Vector3Subtract(asteroid.Position, splitOffset);

        leftAsteroid.Velocity = Vector3Add(asteroid.Velocity, Vector3Scale(tangent, 1.75f));
        rightAsteroid.Velocity = Vector3Subtract(asteroid.Velocity, Vector3Scale(tangent, 1.75f));

        leftAsteroid.Active = true;
        rightAsteroid.Active = true;

        if (asteroidAmount + 1 < MAX_ASTEROIDS)
        {
            asteroids[id] = leftAsteroid;
            asteroids[asteroidAmount] = rightAsteroid;
            asteroidHitCooldown[asteroidAmount] = now;
            asteroidAmount += 1;
        }
        else
        {
            asteroids[id] = CreateAsteroid();
        }

        return true;
    }

    void SendPacketToAllBut(const void* data, size_t len, int exceptPlayerId)
    {
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (!players[i].Active || i == exceptPlayerId)
                continue;

            wsServer.Send(players[i].ConnId, data, len);
        }
    }

    void SendPacketToOnly(const void* data, size_t len, int playerId)
    {
        wsServer.Send(players[playerId].ConnId, data, len);
    }

    void UpdateScoreboard()
    {
        ScoreboardPacket buffer = {};
        buffer.Command = static_cast<int>(NetworkCommands::UpdateScoreboard);
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            buffer.Scoreboard[i] = players[i].Score;
            std::memset(buffer.Names[i], 0, sizeof(buffer.Names[i]));
            if (players[i].Active)
                std::strncpy(buffer.Names[i], players[i].Name, sizeof(buffer.Names[i]) - 1);
        }

        SendPacketToAllBut(&buffer, sizeof(buffer), -1);
    }

    int GetPlayerId(WebSocketServer::ConnId connId)
    {
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (players[i].Active && players[i].ConnId == connId)
                return i;
        }
        return -1;
    }

    int InitializeNewPlayer(WebSocketServer::ConnId connId)
    {
        int playerId = 0;
        for (; playerId < MAX_PLAYERS; ++playerId)
        {
            if (!players[playerId].Active)
                break;
        }

        if (playerId == MAX_PLAYERS)
        {
            wsServer.Disconnect(connId);
            return -1;
        }

        players[playerId].Active = true;
        players[playerId].ValidPosition = false;
        players[playerId].ConnId = connId;
        std::memset(players[playerId].Name, 0, sizeof(players[playerId].Name));
        players[playerId].Position = { 0.0f, 0.0f, 0.0f };
        players[playerId].Rotation = MatrixIdentity();
        players[playerId].Score = 0;

        PlayerPacket acceptBuffer = {};
        acceptBuffer.Command = static_cast<int>(NetworkCommands::AcceptPlayer);
        acceptBuffer.Id = playerId;
        SendPacketToOnly(&acceptBuffer, sizeof(acceptBuffer), playerId);

        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (i == playerId || !players[i].ValidPosition)
                continue;

            PlayerPacket otherBuffer = {};
            otherBuffer.Command = static_cast<int>(NetworkCommands::AddPlayer);
            otherBuffer.Id = i;
            std::memset(otherBuffer.Name, 0, sizeof(otherBuffer.Name));
            if (players[i].Name[0] != '\0')
                std::strncpy(otherBuffer.Name, players[i].Name, sizeof(otherBuffer.Name) - 1);
            otherBuffer.Position = players[i].Position;
            otherBuffer.Rotation = players[i].Rotation;
            SendPacketToOnly(&otherBuffer, sizeof(otherBuffer), playerId);
        }

        return playerId;
    }

    void DisconnectPlayer(int playerId)
    {
        if (playerId < 0 || playerId >= MAX_PLAYERS)
            return;

        players[playerId].Active = false;
        players[playerId].ConnId = -1;
        players[playerId].Name[0] = '\0';
        players[playerId].Position = { 0.0f, 0.0f, 0.0f };
        players[playerId].Rotation = MatrixIdentity();
        players[playerId].ValidPosition = false;

        PlayerPacket removePlayerPacket = {};
        removePlayerPacket.Command = static_cast<int>(NetworkCommands::RemovePlayer);
        removePlayerPacket.Id = playerId;
        SendPacketToAllBut(&removePlayerPacket, sizeof(removePlayerPacket), -1);
    }

    void UpdateAsteroids(double delta)
    {
        if (asteroidAmount < 10)
        {
            SpawnAsteroids(1);
        }

        for (int i = 0; i < asteroidAmount; ++i)
        {
            AsteroidInfo& asteroid = asteroids[i];
            asteroid.Position = Vector3Add(asteroid.Position, Vector3Scale(asteroid.Velocity, (float)delta));

            Vector3 closestPos = { 0.0f, 0.0f, 0.0f };
            float closestDistSq = MAX_SQR_V3;
            for (int j = 0; j < MAX_PLAYERS; ++j)
            {
                if (!players[j].Active)
                    continue;

                float distSq = Vector3DistanceSqr(players[j].Position, asteroid.Position);
                if (distSq < closestDistSq)
                {
                    closestDistSq = distSq;
                    closestPos = players[j].Position;
                }
            }

            if (asteroid.Position.x > closestPos.x + MAX_ASTEROID_DIST)
                asteroid.Position.x = closestPos.x - MAX_ASTEROID_DIST;
            else if (asteroid.Position.x < closestPos.x - MAX_ASTEROID_DIST)
                asteroid.Position.x = closestPos.x + MAX_ASTEROID_DIST;

            if (asteroid.Position.y > closestPos.y + MAX_ASTEROID_DIST)
                asteroid.Position.y = closestPos.y - MAX_ASTEROID_DIST;
            else if (asteroid.Position.y < closestPos.y - MAX_ASTEROID_DIST)
                asteroid.Position.y = closestPos.y + MAX_ASTEROID_DIST;

            if (asteroid.Position.z > closestPos.z + MAX_ASTEROID_DIST)
                asteroid.Position.z = closestPos.z - MAX_ASTEROID_DIST;
            else if (asteroid.Position.z < closestPos.z - MAX_ASTEROID_DIST)
                asteroid.Position.z = closestPos.z + MAX_ASTEROID_DIST;
        }

        static int networkTickCounter = 0;
        networkTickCounter++;

        // Broadcast network data only 5 times a second (30Hz / 6)
        if (networkTickCounter >= 6)
        {
            AsteroidInfoPacket buffer = {};
            buffer.Command = static_cast<int>(NetworkCommands::UpdateAsteroid);
            memcpy(buffer.AllAsteroids, asteroids, sizeof(asteroids));
            buffer.AsteroidCount = asteroidAmount;

            SendPacketToAllBut(&buffer, sizeof(buffer), -1);

            networkTickCounter = 0;
        }
    }

    void HandlePlayerPacket(WebSocketServer::ConnId connId, const PlayerPacket& received)
    {
        int playerId = GetPlayerId(connId);
        if (playerId == -1)
            return;

        if (received.Command == static_cast<int>(NetworkCommands::UpdateInput))
        {
            if (received.Name[0] != '\0')
            {
                std::memset(players[playerId].Name, 0, sizeof(players[playerId].Name));
                std::strncpy(players[playerId].Name, received.Name, sizeof(players[playerId].Name) - 1);
            }

            bool wasFirstUpdate = !players[playerId].ValidPosition;

            players[playerId].Position = received.Position;
            players[playerId].Rotation = received.Rotation;
            players[playerId].ValidPosition = true;

            // Announce to everyone else only once we know the player's name and
            // position, so they never appear as an unnamed ghost.
            if (wasFirstUpdate)
            {
                PlayerPacket addPacket = {};
                addPacket.Command = static_cast<int>(NetworkCommands::AddPlayer);
                addPacket.Id = playerId;
                std::strncpy(addPacket.Name, players[playerId].Name, sizeof(addPacket.Name) - 1);
                addPacket.Position = players[playerId].Position;
                addPacket.Rotation = players[playerId].Rotation;
                SendPacketToAllBut(&addPacket, sizeof(addPacket), playerId);

                UpdateScoreboard();
            }

            PlayerPacket updatePlayerPacket = {};
            updatePlayerPacket.Command = static_cast<int>(NetworkCommands::UpdatePlayer);
            updatePlayerPacket.Id = playerId;
            std::memset(updatePlayerPacket.Name, 0, sizeof(updatePlayerPacket.Name));
            if (players[playerId].Name[0] != '\0')
                std::strncpy(updatePlayerPacket.Name, players[playerId].Name, sizeof(updatePlayerPacket.Name) - 1);
            updatePlayerPacket.Position = players[playerId].Position;
            updatePlayerPacket.Rotation = players[playerId].Rotation;
            SendPacketToAllBut(&updatePlayerPacket, sizeof(updatePlayerPacket), playerId);
        }
    }

    void OnMessage(WebSocketServer::ConnId connId, const uint8_t* data, size_t len)
    {
        if (len == sizeof(PlayerPacket))
        {
            PlayerPacket received = {};
            memcpy(&received, data, sizeof(PlayerPacket));
            HandlePlayerPacket(connId, received);
        }
        else if (len == sizeof(AsteroidDestroyPacket))
        {
            AsteroidDestroyPacket received = {};
            memcpy(&received, data, sizeof(AsteroidDestroyPacket));

            if (received.Command == static_cast<int>(NetworkCommands::DestroyAsteroid))
            {
                if (received.AsteroidID >= 0 && received.AsteroidID < asteroidAmount &&
                    received.PlayerID >= 0 && received.PlayerID < MAX_PLAYERS)
                {
                    if (BreakAsteroid(received.AsteroidID))
                    {
                        players[received.PlayerID].Score += BASE_SCORE;
                        UpdateScoreboard();
                    }
                }
            }
        }
        else if (len == sizeof(ScoreboardPacket))
        {
            ScoreboardPacket received = {};
            memcpy(&received, data, sizeof(ScoreboardPacket));
            if (received.Command == static_cast<int>(NetworkCommands::ResetScoreboardId))
            {
                for (int i = 0; i < MAX_PLAYERS; ++i)
                {
                    if (players[i].Active && i == received.Id)
                    {
                        players[i].Score = 0;
                        break;
                    }
                }
                UpdateScoreboard();
            }
        }
        else if (len == sizeof(ProjectilePacket))
        {
            ProjectilePacket received = {};
            memcpy(&received, data, sizeof(ProjectilePacket));

            if (received.Command == static_cast<int>(NetworkCommands::FireProjectile))
            {
                SendPacketToAllBut(&received, sizeof(received), received.PlayerID);
            }
        }
    }

    void Run()
    {
        double nextTick = GetClockSeconds();
        while (running.load())
        {
            double now = GetClockSeconds();
            if (now >= nextTick)
            {
                UpdateAsteroids(SERVER_TICK_INTERVAL);
                nextTick += SERVER_TICK_INTERVAL;
            }

            wsServer.Poll(0,
                [this](WebSocketServer::ConnId connId)
                {
                    int playerId = InitializeNewPlayer(connId);
                    if (playerId != -1)
                    {
                        SpawnAsteroids(10);
                        UpdateScoreboard();
                    }
                },
                [this](WebSocketServer::ConnId connId, const uint8_t* data, size_t len)
                {
                    OnMessage(connId, data, len);
                },
                [this](WebSocketServer::ConnId connId)
                {
                    int playerId = GetPlayerId(connId);
                    if (playerId != -1)
                        DisconnectPlayer(playerId);
                });

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

ServerHost::ServerHost() : impl_(new Impl()) {}
ServerHost::~ServerHost() { delete impl_; }

bool ServerHost::Start() { return impl_->Start(); }
void ServerHost::Stop() { impl_->Stop(); }
bool ServerHost::IsRunning() const { return impl_->IsRunning(); }
std::string ServerHost::GetHostAddress() const { return impl_->GetHostAddress(); }
