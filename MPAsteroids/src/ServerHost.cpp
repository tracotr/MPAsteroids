#include "include/ServerHost.h"

#include "include/networking/NetCommon.h"
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
    ENetPeer* Peer = nullptr;
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

        if (enet_initialize() != 0)
            return false;

        ENetAddress address = {};
        address.host = ENET_HOST_ANY;
        address.port = SERVER_PORT;

        host = enet_host_create(&address, MAX_PLAYERS, 3, 0, 0);
        if (host == nullptr)
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

        if (host != nullptr)
        {
            enet_host_destroy(host);
            host = nullptr;
        }

        enet_deinitialize();
    }

    bool IsRunning() const { return running.load(); }
    std::string GetHostAddress() const { return "127.0.0.1"; }

private:
    std::atomic<bool> running;
    std::thread worker;
    ENetHost* host = nullptr;
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

    void SendPacketToAllBut(ENetPacket* packet, int exceptPlayerId, int channel)
    {
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (!players[i].Active || i == exceptPlayerId)
                continue;

            ENetPacket* cloned = enet_packet_create(packet->data, packet->dataLength, packet->flags);
            enet_peer_send(players[i].Peer, channel, cloned);
        }
    }

    void SendPacketToOnly(ENetPacket* packet, int playerId, int channel)
    {
        ENetPacket* cloned = enet_packet_create(packet->data, packet->dataLength, packet->flags);
        enet_peer_send(players[playerId].Peer, channel, cloned);
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

        ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_RELIABLE);
        SendPacketToAllBut(packet, -1, 0);
    }

    int GetPlayerId(ENetPeer* peer)
    {
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (players[i].Active && players[i].Peer == peer)
                return i;
        }
        return -1;
    }

    int InitializeNewPlayer(ENetPeer* peer, const char* name = nullptr)
    {
        int playerId = 0;
        for (; playerId < MAX_PLAYERS; ++playerId)
        {
            if (!players[playerId].Active)
                break;
        }

        if (playerId == MAX_PLAYERS)
        {
            enet_peer_disconnect(peer, 0);
            return -1;
        }

        players[playerId].Active = true;
        players[playerId].ValidPosition = false;
        players[playerId].Peer = peer;
        std::memset(players[playerId].Name, 0, sizeof(players[playerId].Name));
        if (name != nullptr && name[0] != '\0')
            std::strncpy(players[playerId].Name, name, sizeof(players[playerId].Name) - 1);
        players[playerId].Position = { 0.0f, 0.0f, 0.0f };
        players[playerId].Rotation = MatrixIdentity();
        players[playerId].Score = 0;

        PlayerPacket acceptBuffer = {};
        acceptBuffer.Command = static_cast<int>(NetworkCommands::AcceptPlayer);
        acceptBuffer.Id = playerId;
        std::memset(acceptBuffer.Name, 0, sizeof(acceptBuffer.Name));
        if (players[playerId].Name[0] != '\0')
            std::strncpy(acceptBuffer.Name, players[playerId].Name, sizeof(acceptBuffer.Name) - 1);
        ENetPacket* acceptPacket = enet_packet_create(&acceptBuffer, sizeof(acceptBuffer), ENET_PACKET_FLAG_RELIABLE);
        SendPacketToOnly(acceptPacket, playerId, 0);

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
            ENetPacket* otherPacket = enet_packet_create(&otherBuffer, sizeof(otherBuffer), ENET_PACKET_FLAG_RELIABLE);
            SendPacketToOnly(otherPacket, playerId, 0);
        }

        PlayerPacket allOtherBuffer = {};
        allOtherBuffer.Command = static_cast<int>(NetworkCommands::AddPlayer);
        allOtherBuffer.Id = playerId;
        std::memset(allOtherBuffer.Name, 0, sizeof(allOtherBuffer.Name));
        if (players[playerId].Name[0] != '\0')
            std::strncpy(allOtherBuffer.Name, players[playerId].Name, sizeof(allOtherBuffer.Name) - 1);
        allOtherBuffer.Position = players[playerId].Position;
        allOtherBuffer.Rotation = players[playerId].Rotation;
        ENetPacket* eventPacket = enet_packet_create(&allOtherBuffer, sizeof(allOtherBuffer), ENET_PACKET_FLAG_RELIABLE);
        SendPacketToAllBut(eventPacket, playerId, 0);

        return playerId;
    }

    void DisconnectPlayer(int playerId)
    {
        if (playerId < 0 || playerId >= MAX_PLAYERS)
            return;

        players[playerId].Active = false;
        players[playerId].Peer = nullptr;
        players[playerId].Name[0] = '\0';
        players[playerId].Position = { 0.0f, 0.0f, 0.0f };
        players[playerId].Rotation = MatrixIdentity();
        players[playerId].ValidPosition = false;

        PlayerPacket removePlayerPacket = {};
        removePlayerPacket.Command = static_cast<int>(NetworkCommands::RemovePlayer);
        removePlayerPacket.Id = playerId;
        ENetPacket* removePacket = enet_packet_create(&removePlayerPacket, sizeof(removePlayerPacket), ENET_PACKET_FLAG_RELIABLE);
        SendPacketToAllBut(removePacket, -1, 0);
    }

    void UpdateAsteroids(double delta)
    {
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

        AsteroidInfoPacket buffer = {};
        buffer.Command = static_cast<int>(NetworkCommands::UpdateAsteroid);
        memcpy(buffer.AllAsteroids, asteroids, sizeof(asteroids));
        buffer.AsteroidCount = asteroidAmount;

        ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
        SendPacketToAllBut(packet, -1, 1);
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

            ENetEvent event = {};
            if (enet_host_service(host, &event, 0) > 0)
            {
                switch (event.type)
                {
                    case ENET_EVENT_TYPE_CONNECT:
                    {
                        // Initialize player immediately on connection (like original server)
                        // Name will be updated from first UpdateInput packet
                        int playerId = InitializeNewPlayer(event.peer, nullptr);
                        if (playerId != -1)
                        {
                            SpawnAsteroids(10);
                            UpdateScoreboard();
                        }
                        break;
                    }
                    case ENET_EVENT_TYPE_RECEIVE:
                    {
                        if (event.packet->dataLength == sizeof(PlayerPacket))
                        {
                            PlayerPacket received = {};
                            memcpy(&received, event.packet->data, sizeof(PlayerPacket));
                            int playerId = GetPlayerId(event.peer);
                            
                            if (playerId == -1)
                            {
                                enet_peer_disconnect(event.peer, 0);
                                enet_packet_destroy(event.packet);
                                break;
                            }

                            if (received.Command == static_cast<int>(NetworkCommands::UpdateInput))
                            {
                                // Update player name from first/subsequent packets
                                if (received.Name[0] != '\0')
                                {
                                    std::memset(players[playerId].Name, 0, sizeof(players[playerId].Name));
                                    std::strncpy(players[playerId].Name, received.Name, sizeof(players[playerId].Name) - 1);
                                }

                                players[playerId].Position = received.Position;
                                players[playerId].Rotation = received.Rotation;
                                players[playerId].ValidPosition = true;

                                PlayerPacket updatePlayerPacket = {};
                                updatePlayerPacket.Command = static_cast<int>(NetworkCommands::UpdatePlayer);
                                updatePlayerPacket.Id = playerId;
                                std::memset(updatePlayerPacket.Name, 0, sizeof(updatePlayerPacket.Name));
                                if (players[playerId].Name[0] != '\0')
                                    std::strncpy(updatePlayerPacket.Name, players[playerId].Name, sizeof(updatePlayerPacket.Name) - 1);
                                updatePlayerPacket.Position = players[playerId].Position;
                                updatePlayerPacket.Rotation = players[playerId].Rotation;
                                ENetPacket* packet = enet_packet_create(&updatePlayerPacket, sizeof(updatePlayerPacket), 0);
                                SendPacketToAllBut(packet, playerId, 2);
                            }
                        }
                        else if (event.packet->dataLength == sizeof(AsteroidDestroyPacket))
                        {
                            AsteroidDestroyPacket received = {};
                            memcpy(&received, event.packet->data, sizeof(AsteroidDestroyPacket));

                            if (received.Command == static_cast<int>(NetworkCommands::DestroyAsteroid))
                            {
                                if (received.AsteroidID >= 0 && received.AsteroidID < asteroidAmount)
                                {
                                    if (BreakAsteroid(received.AsteroidID)) 
                                    {
                                        players[received.PlayerID].Score += BASE_SCORE;
                                        UpdateScoreboard();
                                    }
                                }
                            }
                        }
                        else if (event.packet->dataLength == sizeof(ScoreboardPacket))
                        {
                            ScoreboardPacket received = {};
                            memcpy(&received, event.packet->data, sizeof(ScoreboardPacket));
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
                        else if (event.packet->dataLength == sizeof(ProjectilePacket))
                        {
                            ProjectilePacket received = {};
                            memcpy(&received, event.packet->data, sizeof(ProjectilePacket));

                            if (received.Command == static_cast<int>(NetworkCommands::FireProjectile))
                            {
                                // send the projectile event to all other clients
                                ENetPacket* packet = enet_packet_create(&received, sizeof(ProjectilePacket), ENET_PACKET_FLAG_RELIABLE);
                                SendPacketToAllBut(packet, received.PlayerID, 0);
                            }
                        }

                        enet_packet_destroy(event.packet);
                        break;
                    }
                    case ENET_EVENT_TYPE_DISCONNECT:
                    case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                    {
                        int playerId = GetPlayerId(event.peer);
                        if (playerId == -1)
                            break;
                        DisconnectPlayer(playerId);
                        break;
                    }
                    default:
                        break;
                }
            }

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
