#include "include/ServerHost.h"

#include "include/WebSocketServer.h"
#include "include/networking/NetConstants.h"
#include "include/Upgrades.h"

#include <cstdio>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>
#include <string>
#include <thread>

// Bots kept per real player, and the ceiling on the total.
#define BOTS_PER_PLAYER 2
#define MAX_BOTS 12

// How fast a bot turns onto a target, in radians per second.
#define BOT_TURN_RATE 2.2f

// How far a bot can see, and how close it must be to shoot.
#define BOT_SIGHT_RANGE 70.0f
#define BOT_FIRE_RANGE 55.0f

// How closely the nose must line up before firing. A cosine, so about 26 degrees.
#define BOT_AIM_DOT 0.90f

// Fraction of bot shots allowed to land. The rest deliberately miss.
#define BOT_ACCURACY 0.6f

// Bots fly slower than their build allows, so a player can always run.
#define BOT_SPEED_SCALE 0.7f

// Where a bot turns back, kept inside the barrier.
#define BOT_LEASH_RADIUS (WORLD_RADIUS * 0.75f)

// Seconds between bot trigger pulls, randomised so they do not fire in unison.
#define BOT_FIRE_MIN_GAP 0.55
#define BOT_FIRE_MAX_GAP 1.1

// Stand-ins for the model sizes the server cannot measure.
#define BOT_ASTEROID_RADIUS 1.15f
#define BOT_SHIP_RADIUS 1.0f

// How close a rock gets before a bot steers around it.
#define BOT_AVOID_RANGE 14.0f

// How long a bot stays on one target, so it is not re-aiming every tick.
#define BOT_TARGET_HOLD 4.0

// Rocks nearer than this are obstacles to fly around, not targets to shoot.
#define BOT_MIN_TARGET_RANGE 16.0f

#define BASE_SCORE 5

// A player is a harder target than a rock, so worth more.
#define KILL_SCORE 25

// Not the same number as score.
#define BASE_XP 5
#define KILL_XP 25

// For testing; defaults to the real game so an ordinary build cannot ship it.
//   make server EXTRA_SERVER_DEFINES=-DXP_MULTIPLIER=20
#ifndef XP_MULTIPLIER
#define XP_MULTIPLIER 3
#endif

// What a full-sized rock does to a health it runs into.
#define ASTEROID_RAM_DAMAGE 130.0f

// What a laser is worth against a ship. Stops the base gun one-shotting a base ship.
#define PVP_DAMAGE_SCALE 0.4f

// How many trigger pulls of credit a ship can bank
const float FIRE_BURST_VOLLEYS = 2.0f;

// How often a regenerating health tells its owner about it.
const double HEALTH_REPORT_INTERVAL = 0.2;

const double SERVER_TICK_RATE = 20.0;
const double SERVER_TICK_INTERVAL = 1.0 / SERVER_TICK_RATE;

// Asteroid positions are sent every Nth tick, and clients keep the rocks moving in between.
const int ASTEROID_BROADCAST_EVERY = 2; // 20Hz / 2 = 10Hz

// Cap on asteroids created in a single tick.
const int ASTEROID_SPAWNS_PER_TICK = 4;

// Applies a bounty, rounded 
static int WithBounty(int amount, float multiplier)
{
    return (int)(amount * multiplier + 0.5f);
}

struct ServerPlayer
{
    bool Active = false;
    bool ValidPosition = false;
    WebSocketServer::ConnId ConnId = -1;
    char Name[MAX_PLAYER_NAME_LENGTH] = { 0 };
    Vector3 Position = { 0.0f, 0.0f, 0.0f };
    Matrix Rotation = MatrixIdentity();
    int Score = 0;

    UpgradeState Upgrades;
    float Health = 100.0f;

    // Seeded per player, so two people levelling in the same instant are not handed the same three cards.
    uint32_t OfferRng = 0;

    // When they last reported in, and when they were last killed.
    double LastInputTime = 0.0;
    double KilledAt = -1.0;

    // When something last took health off them.
    double LastDamageTime = -1000.0;

    // Credit for firing, in lasers. Spent one per laser and refilled 
    float FireTokens = 0.0f;

    // When their own health was last reported to them.
    double LastHealthReport = -1000.0;

    // Bots only. The death already respawned from, so one death moves the ship once.
    double BotHandledKillAt = -1.0;

    // Bots only. Where it heads when nothing is worth shooting.
    Vector3 BotWander = { 0.0f, 0.0f, 1.0f };

    // Bots only. The earliest it may pull the trigger again.
    double BotNextFireAt = 0.0;

    // Bots only. What it has settled on shooting, and until when.
    uint32_t BotTargetAsteroid = 0;
    int BotTargetPlayer = -1;
    double BotTargetUntil = 0.0;

    bool IsBot = false;
};

// One switched-on cube of the play area.
struct RegionCell
{
    int X = 0, Y = 0, Z = 0;
    double LastOccupied = 0.0;
};

// The server's own record of an asteroid, separate from the version sent to clients.
struct ServerAsteroid
{
    bool Active = false;
    uint32_t Id = 0;
    uint8_t Seed = 0;
    Vector3 Position = { 0.0f, 0.0f, 0.0f };
    Vector3 Velocity = { 0.0f, 0.0f, 0.0f };
    float Scale = 1.0f;

    // Worked out from Scale whenever the rock is made or resized.
    float Health = 0.0f;

    double LastHitTime = -1.0;
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

    ServerAsteroid asteroids[MAX_ASTEROIDS] = {};
    int liveAsteroids = 0;

    RegionCell regionCells[MAX_REGION_CELLS] = {};
    int regionCellCount = 0;

    // Never reused, so a client can tell an asteroid that moved apart from a different one that took over its old slot.
    uint32_t nextAsteroidId = 1;

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

    // Throws away points outside the sphere and tries again, which spreads the directions evenly.
    Vector3 RandomDirection()
    {
        for (int attempt = 0; attempt < 12; ++attempt)
        {
            Vector3 candidate = { RandBetween(-1.0f, 1.0f), RandBetween(-1.0f, 1.0f), RandBetween(-1.0f, 1.0f) };
            float lengthSq = Vector3LengthSqr(candidate);
            if (lengthSq > 0.0001f && lengthSq <= 1.0f)
                return Vector3Scale(candidate, 1.0f / sqrtf(lengthSq));
        }
        return (Vector3){ 0.0f, 0.0f, 1.0f };
    }

    // Counts real people. Bots hold player slots, so they have to be skipped.
    int HumanCount() const
    {
        int count = 0;
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (players[i].Active && !players[i].IsBot)
                count++;
        }
        return count;
    }

    int BotCount() const
    {
        int count = 0;
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (players[i].Active && players[i].IsBot)
                count++;
        }
        return count;
    }

    // Bots fill from the top down, people from the bottom up.
    int AllocateBotSlot() const
    {
        for (int i = MAX_PLAYERS - 1; i >= 0; --i)
        {
            if (!players[i].Active)
                return i;
        }
        return -1;
    }

    // Frees the highest bot slot so an arriving player is never turned away.
    int EvictOneBot()
    {
        for (int i = MAX_PLAYERS - 1; i >= 0; --i)
        {
            if (players[i].Active && players[i].IsBot)
            {
                DisconnectPlayer(i);
                return i;
            }
        }
        return -1;
    }

    int AllocateAsteroid()
    {
        for (int i = 0; i < MAX_ASTEROIDS; ++i)
        {
            if (asteroids[i].Active)
                continue;

            asteroids[i] = ServerAsteroid{};
            asteroids[i].Active = true;
            asteroids[i].Id = nextAsteroidId++;
            asteroids[i].Seed = (uint8_t)(asteroids[i].Id * 37u);
            liveAsteroids++;
            return i;
        }
        return -1;
    }

    void ReleaseAsteroid(int slot)
    {
        if (slot < 0 || slot >= MAX_ASTEROIDS || !asteroids[slot].Active)
            return;

        asteroids[slot] = ServerAsteroid{};
        liveAsteroids--;
    }

    int FindAsteroidById(uint32_t id) const
    {
        if (id == 0)
            return -1;

        for (int i = 0; i < MAX_ASTEROIDS; ++i)
        {
            if (asteroids[i].Active && asteroids[i].Id == id)
                return i;
        }
        return -1;
    }

    // --- The play area: cubes switch on where players are, off once empty ------
    // Asteroids are kept inside whatever is switched on.

    static int CellIndexOf(float value)
    {
        return (int)floorf(value / REGION_CELL_SIZE);
    }

    int FindRegionCell(int x, int y, int z) const
    {
        for (int i = 0; i < regionCellCount; ++i)
        {
            if (regionCells[i].X == x && regionCells[i].Y == y && regionCells[i].Z == z)
                return i;
        }
        return -1;
    }

    void TouchRegionCell(int x, int y, int z, double now)
    {
        int existing = FindRegionCell(x, y, z);
        if (existing != -1)
        {
            regionCells[existing].LastOccupied = now;
            return;
        }

        if (regionCellCount >= MAX_REGION_CELLS)
            return;

        regionCells[regionCellCount].X = x;
        regionCells[regionCellCount].Y = y;
        regionCells[regionCellCount].Z = z;
        regionCells[regionCellCount].LastOccupied = now;
        regionCellCount++;
    }

    // Switches on the eight cubes meeting at the nearest grid corner.
    void TouchRegionAround(const Vector3& position, double now)
    {
        const int cx = (int)roundf(position.x / REGION_CELL_SIZE);
        const int cy = (int)roundf(position.y / REGION_CELL_SIZE);
        const int cz = (int)roundf(position.z / REGION_CELL_SIZE);

        for (int dx = -1; dx <= 0; ++dx)
            for (int dy = -1; dy <= 0; ++dy)
                for (int dz = -1; dz <= 0; ++dz)
                    TouchRegionCell(cx + dx, cy + dy, cz + dz, now);
    }

    int RegionCellOf(const Vector3& position) const
    {
        return FindRegionCell(CellIndexOf(position.x), CellIndexOf(position.y), CellIndexOf(position.z));
    }

    void PruneRegionCells(double now)
    {
        for (int i = regionCellCount - 1; i >= 0; --i)
        {
            if (now - regionCells[i].LastOccupied < REGION_CELL_TTL)
                continue;

            regionCells[i] = regionCells[regionCellCount - 1];
            regionCellCount--;
        }
    }

    bool IsInsideRegion(const Vector3& position) const
    {
        return FindRegionCell(CellIndexOf(position.x), CellIndexOf(position.y), CellIndexOf(position.z)) != -1;
    }

    Vector3 RandomPointInCell(int index)
    {
        if (index < 0 || index >= regionCellCount)
            return (Vector3){ 0.0f, 0.0f, 0.0f };

        const RegionCell& cell = regionCells[index];
        return (Vector3){
            ((float)cell.X + RandBetween(0.05f, 0.95f)) * REGION_CELL_SIZE,
            ((float)cell.Y + RandBetween(0.05f, 0.95f)) * REGION_CELL_SIZE,
            ((float)cell.Z + RandBetween(0.05f, 0.95f)) * REGION_CELL_SIZE
        };
    }

    float DistanceToNearestPlayer(const Vector3& position) const
    {
        float closest = MAX_SQR_V3;
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (!players[i].Active || !players[i].ValidPosition)
                continue;

            float distSq = Vector3DistanceSqr(players[i].Position, position);
            if (distSq < closest) closest = distSq;
        }
        return (closest == MAX_SQR_V3) ? -1.0f : sqrtf(closest);
    }

    // Looks for a spot in the cube with room around it.
    Vector3 PickPlacementInCell(int index)
    {
        Vector3 best = RandomPointInCell(index);
        float bestClearance = DistanceToNearestPlayer(best);

        for (int attempt = 1; attempt < 10 && bestClearance < ASTEROID_PLACEMENT_CLEARANCE; ++attempt)
        {
            Vector3 candidate = RandomPointInCell(index);
            float clearance = DistanceToNearestPlayer(candidate);

            // No players positioned yet, so anywhere will do.
            if (clearance < 0.0f)
                return candidate;

            if (clearance > bestClearance)
            {
                best = candidate;
                bestClearance = clearance;
            }
        }

        return best;
    }

    int DesiredAsteroidCount() const
    {
        // Scales with how much world is switched on, not with how many players are connected.
        int desired = regionCellCount * ASTEROID_PER_CELL;
        if (desired > ASTEROID_MAX_TOTAL) desired = ASTEROID_MAX_TOTAL;
        if (desired > MAX_ASTEROIDS) desired = MAX_ASTEROIDS;
        return desired;
    }

    bool SpawnAsteroidInCell(int cellIndex)
    {
        int slot = AllocateAsteroid();
        if (slot == -1)
            return false;

        ServerAsteroid& asteroid = asteroids[slot];
        asteroid.Position = PickPlacementInCell(cellIndex);
        asteroid.Velocity = Vector3Scale(RandomDirection(), RandBetween(1.2f, 3.2f));
        asteroid.Scale = RandBetween(0.85f, 1.45f);
        asteroid.Health = AsteroidHealthForScale(asteroid.Scale);
        return true;
    }

    int EmptiestRegionCell()
    {
        if (regionCellCount == 0)
            return -1;

        int perCell[MAX_REGION_CELLS] = { 0 };
        for (int i = 0; i < MAX_ASTEROIDS; ++i)
        {
            if (!asteroids[i].Active)
                continue;

            int cell = RegionCellOf(asteroids[i].Position);
            if (cell >= 0) perCell[cell]++;
        }

        int emptiest = 0;
        for (int i = 1; i < regionCellCount; ++i)
        {
            if (perCell[i] < perCell[emptiest]) emptiest = i;
        }
        return emptiest;
    }

    // Tops the field up, always filling whichever cube has fewest.
    void MaintainAsteroidPopulation()
    {
        const int desired = DesiredAsteroidCount();
        if (liveAsteroids >= desired || regionCellCount == 0)
            return;

        int perCell[MAX_REGION_CELLS] = { 0 };
        for (int i = 0; i < MAX_ASTEROIDS; ++i)
        {
            if (!asteroids[i].Active)
                continue;

            int cell = RegionCellOf(asteroids[i].Position);
            if (cell >= 0) perCell[cell]++;
        }

        for (int n = 0; n < ASTEROID_SPAWNS_PER_TICK && liveAsteroids < desired; ++n)
        {
            int emptiest = 0;
            for (int i = 1; i < regionCellCount; ++i)
            {
                if (perCell[i] < perCell[emptiest]) emptiest = i;
            }

            if (!SpawnAsteroidInCell(emptiest))
                break;

            perCell[emptiest]++;
        }
    }

    // Takes a bite out of a rock, breaking it only once nothing is left.
    bool DamageAsteroid(uint32_t asteroidId, float damage)
    {
        int slot = FindAsteroidById(asteroidId);
        if (slot == -1)
            return false;

        asteroids[slot].Health -= damage;
        asteroids[slot].LastHitTime = GetClockSeconds();

        if (asteroids[slot].Health > 0.0f)
            return false;

        return BreakAsteroid(asteroidId);
    }

    bool BreakAsteroid(uint32_t asteroidId)
    {
        int slot = FindAsteroidById(asteroidId);
        if (slot == -1)
            return false;

        double now = GetClockSeconds();

        ServerAsteroid parent = asteroids[slot];

        // Freed before the fragments are allocated.
        ReleaseAsteroid(slot);

        float childScale = parent.Scale * ASTEROID_SPLIT_FACTOR;
        if (childScale < MIN_ASTEROID_SCALE)
            return true;

        // The two pieces are pushed out sideways from the direction the rock was going
        Vector3 tangent = Vector3CrossProduct(parent.Velocity, RandomDirection());
        if (Vector3LengthSqr(tangent) < 0.0001f)
            tangent = (Vector3){ 1.0f, 0.0f, 0.0f };

        tangent = Vector3Normalize(tangent);

        Vector3 separation = Vector3Scale(tangent, parent.Scale * 1.1f);
        const float kick = 1.75f;

        for (int side = 0; side < 2; ++side)
        {
            int childSlot = AllocateAsteroid();
            if (childSlot == -1)
                break;

            float sign = (side == 0) ? 1.0f : -1.0f;
            ServerAsteroid& child = asteroids[childSlot];
            child.Position = Vector3Add(parent.Position, Vector3Scale(separation, sign));
            child.Velocity = Vector3Add(parent.Velocity, Vector3Scale(tangent, kick * sign));
            child.Scale = childScale;
            child.Health = AsteroidHealthForScale(childScale);
            child.LastHitTime = now;
        }

        return true;
    }

    void SendPacketToAllBut(const void* data, size_t len, int exceptPlayerId)
    {
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (!players[i].Active || players[i].IsBot || i == exceptPlayerId)
                continue;

            wsServer.Send(players[i].ConnId, data, len);
        }
    }

    void SendPacketToOnly(const void* data, size_t len, int playerId)
    {
        if (!players[playerId].Active || players[playerId].IsBot)
            return;
        
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

    // Everything below stamps or derives from server-held state.

    // The fields a client may not describe about itself.
    void StampPlayerState(PlayerPacket& packet, int playerId)
    {
        packet.Health = players[playerId].Health;
        packet.MaxHealth = players[playerId].Upgrades.Stats().MaxHealth;
        packet.Level = (uint8_t)players[playerId].Upgrades.Level();
        packet.Evolution = players[playerId].Upgrades.WeaponEvolution();
    }

    void SendHealth(int playerId, double now)
    {
        if (!players[playerId].Active || players[playerId].IsBot)
            return;

        players[playerId].LastHealthReport = now;

        PlayerHealthPacket buffer = {};
        buffer.Command = static_cast<int>(NetworkCommands::UpdateHealth);
        buffer.Health = players[playerId].Health;
        buffer.MaxHealth = players[playerId].Upgrades.Stats().MaxHealth;
        SendPacketToOnly(&buffer, sizeof(buffer), playerId);
    }

    // Rolls a fresh offer first if one is owed.
    void SendUpgradeState(int playerId)
    {
        if (!players[playerId].Active || players[playerId].IsBot)
            return;
        
        UpgradeState& state = players[playerId].Upgrades;

        if (state.PendingPicks() > 0 && state.OfferCount() == 0)
            state.RollOffer(players[playerId].OfferRng);

        UpgradeStatePacket buffer = {};
        state.WriteTo(buffer);
        SendPacketToOnly(&buffer, sizeof(buffer), playerId);
    }

    // All experience is awarded here
    void GrantXp(int playerId, int amount)
    {
        const float multiplier = players[playerId].Upgrades.Stats().ScoreMultiplier;

        // Levelling only ever hands out a pick.
        players[playerId].Upgrades.AddXp(WithBounty(amount, multiplier) * XP_MULTIPLIER);
        SendUpgradeState(playerId);
    }

    void AwardScore(int playerId, int base)
    {
        players[playerId].Score += WithBounty(base, players[playerId].Upgrades.Stats().ScoreMultiplier);
    }

    void KillPlayer(int victimId, int killerId, double now)
    {
        players[victimId].KilledAt = now;

        // Dying costs the whole score but only half the build
        players[victimId].Score = 0;
        players[victimId].Upgrades.ApplyDeathPenalty();
        players[victimId].Health = players[victimId].Upgrades.Stats().MaxHealth;
        players[victimId].LastDamageTime = -1000.0;
        players[victimId].FireTokens = 0.0f;

        if (killerId >= 0 && killerId < MAX_PLAYERS && players[killerId].Active)
        {
            AwardScore(killerId, KILL_SCORE);
            GrantXp(killerId, KILL_XP);
        }

        // The victim is told so it can respawn
        PlayerKillPacket notify = {};
        notify.Command = static_cast<int>(NetworkCommands::PlayerKilled);
        notify.KillerId = killerId;
        notify.VictimId = victimId;
        SendPacketToOnly(&notify, sizeof(notify), victimId);

        SendUpgradeState(victimId);
        SendHealth(victimId, now);
        UpdateScoreboard();
    }

    // Every point of damage arrives here.
    float LaserDamageToPlayer(int shooterId) const
    {
        return players[shooterId].Upgrades.Stats().Damage * PVP_DAMAGE_SCALE;
    }

    void ApplyDamage(int victimId, float amount, int killerId, double now)
    {
        amount *= players[victimId].Upgrades.Stats().DamageTakenScale;

        if (amount <= 0.0f)
            return;

        players[victimId].Health -= amount;
        players[victimId].LastDamageTime = now;

        if (players[victimId].Health > 0.0f)
        {
            SendHealth(victimId, now);
            return;
        }

        KillPlayer(victimId, killerId, now);
    }

    // Stops an abandoned ship being farmed: a client not running frames never processes its respawn.
    bool CanBeHurt(int victimId, double now) const
    {
        if (victimId < 0 || victimId >= MAX_PLAYERS || !players[victimId].Active)
            return false;

        if (now - players[victimId].LastInputTime > PLAYER_STALE_SECONDS)
            return false;

        if (players[victimId].KilledAt > 0.0 && now - players[victimId].KilledAt < KILL_COOLDOWN_SECONDS)
            return false;

        return true;
    }

    // Health comes back only after a stretch with nothing hitting us.
    void UpdatePlayers(double delta)
    {
        const double now = GetClockSeconds();

        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (!players[i].Active || !players[i].ValidPosition)
                continue;

            const ShipStats& stats = players[i].Upgrades.Stats();

            // Credit builds up even while nobody is firing.
            const float perSecond = (float)(1.0 / stats.FireCooldown);
            const float ceiling = FIRE_BURST_VOLLEYS;
            players[i].FireTokens += perSecond * (float)delta;
            if (players[i].FireTokens > ceiling)
                players[i].FireTokens = ceiling;

            if (stats.Regen <= 0.0f) continue;
            if (players[i].Health >= stats.MaxHealth) continue;
            if (now - players[i].LastDamageTime < REGEN_DELAY_SECONDS) continue;

            players[i].Health += stats.Regen * (float)delta;
            if (players[i].Health > stats.MaxHealth)
                players[i].Health = stats.MaxHealth;

            if (now - players[i].LastHealthReport >= HEALTH_REPORT_INTERVAL)
                SendHealth(i, now);
        }
    }

    // Builds a rotation facing this way. The ship model faces -Z, hence the negation.
    static Matrix RotationFacing(Vector3 forward)
    {
        if (Vector3LengthSqr(forward) < 0.0001f)
            return MatrixIdentity();

        forward = Vector3Normalize(forward);

        Vector3 zAxis = Vector3Negate(forward);
        Vector3 upHint = (fabsf(zAxis.y) > 0.99f) ? (Vector3){ 1.0f, 0.0f, 0.0f }
                                                  : (Vector3){ 0.0f, 1.0f, 0.0f };

        Vector3 xAxis = Vector3Normalize(Vector3CrossProduct(upHint, zAxis));
        Vector3 yAxis = Vector3CrossProduct(zAxis, xAxis);

        Matrix result = MatrixIdentity();
        result.m0 = xAxis.x; result.m1 = xAxis.y; result.m2 = xAxis.z;
        result.m4 = yAxis.x; result.m5 = yAxis.y; result.m6 = yAxis.z;
        result.m8 = zAxis.x; result.m9 = zAxis.y; result.m10 = zAxis.z;
        return result;
    }

    // Reads which way a ship faces. The model faces -Z.
    static Vector3 FacingOf(const Matrix& rotation)
    {
        return Vector3Normalize(Vector3Transform((Vector3){ 0.0f, 0.0f, -1.0f }, rotation));
    }

    // Takes a random card from the offer, since a bot has no screen to choose on.
    void TakeBotUpgrades(int botId)
    {
        UpgradeState& state = players[botId].Upgrades;

        // Bounded, so a card that will not apply cannot spin the whole tick.
        for (int guard = 0; guard < MAX_UPGRADE_PICKS && state.PendingPicks() > 0; ++guard)
        {
            if (state.OfferCount() == 0)
                state.RollOffer(players[botId].OfferRng);

            if (state.OfferCount() == 0)
                break;

            const int index = (int)RandBetween(0.0f, (float)state.OfferCount() - 0.001f);
            if (!state.Choose(state.OfferedId(index)))
                break;

            players[botId].Health = state.Stats().MaxHealth;
        }
    }

    // Applies ram damage to a bot, which has no client to report the collision.
    bool ResolveBotRam(int botId, double now)
    {
        for (int a = 0; a < MAX_ASTEROIDS; ++a)
        {
            if (!asteroids[a].Active)
                continue;

            const float reach = asteroids[a].Scale * BOT_ASTEROID_RADIUS + BOT_SHIP_RADIUS;
            if (Vector3DistanceSqr(asteroids[a].Position, players[botId].Position) > reach * reach)
                continue;

            const float damage = ASTEROID_RAM_DAMAGE * asteroids[a].Scale;
            const uint32_t id = asteroids[a].Id;

            if (!BreakAsteroid(id))
                return false;

            ApplyDamage(botId, damage, -1, now);
            return true;
        }
        return false;
    }

    // Keeps the bot count at two per player as people come and go.
    void MaintainBotPopulation()
    {
        int wanted = HumanCount() * BOTS_PER_PLAYER;
        if (wanted > MAX_BOTS)
            wanted = MAX_BOTS;

        int have = BotCount();

        for (; have < wanted; ++have)
        {
            if (InitializeNewBot() == -1)
                break;
        }

        // Releases the newest first, so the bot block stays against the top.
        while (have > wanted)
        {
            int newest = -1;
            for (int i = 0; i < MAX_PLAYERS; ++i)
            {
                if (players[i].Active && players[i].IsBot)
                {
                    newest = i;
                    break;
                }
            }

            if (newest == -1)
                break;

            DisconnectPlayer(newest);
            have--;
        }
    }

    // Drives every bot: steering, firing, upgrades and respawn.
    void UpdateBots(double delta)
    {
        const double now = GetClockSeconds();

        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            ServerPlayer& bot = players[i];
            if (!bot.Active || !bot.IsBot)
                continue;

            // Stops CanBeHurt reading a bot as an abandoned ship and making it unkillable.
            bot.LastInputTime = now;

            // Moves the ship after a death, which is what a client does on PlayerKilled.
            if (bot.KilledAt > 0.0 && bot.KilledAt != bot.BotHandledKillAt)
            {
                bot.BotHandledKillAt = bot.KilledAt;
                bot.Position = Vector3Scale(RandomDirection(), RandBetween(8.0f, PLAYER_SPAWN_RADIUS));
                bot.BotWander = RandomDirection();
            }

            TakeBotUpgrades(i);

            const ShipStats& stats = bot.Upgrades.Stats();
            const Vector3 facing = FacingOf(bot.Rotation);

            // --- pick the nearest thing worth shooting ----------------------
            Vector3 targetPoint = { 0.0f, 0.0f, 0.0f };
            Vector3 targetVelocity = { 0.0f, 0.0f, 0.0f };
            float targetDistance = BOT_SIGHT_RANGE;
            int targetPlayer = -1;
            uint32_t targetAsteroid = 0;

            // The nearest rock ahead, tracked separately: it is the thing to fly around.
            Vector3 avoidFrom = { 0.0f, 0.0f, 0.0f };
            float avoidDistance = BOT_AVOID_RANGE;
            bool avoiding = false;

            for (int a = 0; a < MAX_ASTEROIDS; ++a)
            {
                if (!asteroids[a].Active)
                    continue;

                const Vector3 toRock = Vector3Subtract(asteroids[a].Position, bot.Position);
                const float distance = Vector3Length(toRock);

                // This close it is an obstacle, left to the avoidance below.
                if (distance > BOT_MIN_TARGET_RANGE && distance < targetDistance)
                {
                    targetDistance = distance;
                    targetPoint = asteroids[a].Position;
                    targetVelocity = asteroids[a].Velocity;
                    targetAsteroid = asteroids[a].Id;
                    targetPlayer = -1;
                }

                // Only what is ahead is worth dodging.
                if (distance > 0.0001f
                    && Vector3DotProduct(facing, Vector3Scale(toRock, 1.0f / distance)) > 0.2f)
                {
                    const float clearance = distance - asteroids[a].Scale * BOT_ASTEROID_RADIUS;
                    if (clearance < avoidDistance)
                    {
                        avoidDistance = clearance;
                        avoidFrom = asteroids[a].Position;
                        avoiding = true;
                    }
                }
            }

            for (int other = 0; other < MAX_PLAYERS; ++other)
            {
                if (other == i || !players[other].Active || !players[other].ValidPosition)
                    continue;

                // Bots leave each other alone and only ever target people.
                if (players[other].IsBot)
                    continue;

                const float distance = Vector3Distance(bot.Position, players[other].Position);
                if (distance < targetDistance)
                {
                    targetDistance = distance;
                    targetPoint = players[other].Position;

                    // Players are not led: the server never sees their velocity.
                    targetVelocity = (Vector3){ 0.0f, 0.0f, 0.0f };
                    targetPlayer = other;
                    targetAsteroid = 0;
                }
            }

            // A target already settled on beats whatever is nearest this tick.
            if (now < bot.BotTargetUntil)
            {
                bool held = false;

                if (bot.BotTargetAsteroid != 0)
                {
                    const int slot = FindAsteroidById(bot.BotTargetAsteroid);
                    if (slot != -1)
                    {
                        const float distance = Vector3Distance(bot.Position, asteroids[slot].Position);
                        if (distance > BOT_MIN_TARGET_RANGE && distance < BOT_SIGHT_RANGE)
                        {
                            targetPoint = asteroids[slot].Position;
                            targetVelocity = asteroids[slot].Velocity;
                            targetDistance = distance;
                            targetAsteroid = asteroids[slot].Id;
                            targetPlayer = -1;
                            held = true;
                        }
                    }
                }
                else if (bot.BotTargetPlayer >= 0
                      && players[bot.BotTargetPlayer].Active
                      && players[bot.BotTargetPlayer].ValidPosition
                      && !players[bot.BotTargetPlayer].IsBot)
                {
                    const float distance = Vector3Distance(bot.Position, players[bot.BotTargetPlayer].Position);
                    if (distance < BOT_SIGHT_RANGE)
                    {
                        targetPoint = players[bot.BotTargetPlayer].Position;
                        targetVelocity = (Vector3){ 0.0f, 0.0f, 0.0f };
                        targetDistance = distance;
                        targetPlayer = bot.BotTargetPlayer;
                        targetAsteroid = 0;
                        held = true;
                    }
                }

                // Shot to pieces or left behind, so pick again.
                if (!held)
                    bot.BotTargetUntil = 0.0;
            }

            // Nothing held, so take what the scan found and stay on it.
            if (now >= bot.BotTargetUntil && (targetAsteroid != 0 || targetPlayer != -1))
            {
                bot.BotTargetAsteroid = targetAsteroid;
                bot.BotTargetPlayer = targetPlayer;
                bot.BotTargetUntil = now + BOT_TARGET_HOLD;
            }

            const bool hasTarget = (targetPlayer != -1 || targetAsteroid != 0);

            // Aims where the rock will be, since the laser takes time to arrive.
            Vector3 aimPoint = targetPoint;
            if (hasTarget && stats.LaserSpeed > 0.01f)
            {
                const float flightTime = targetDistance / stats.LaserSpeed;
                aimPoint = Vector3Add(targetPoint, Vector3Scale(targetVelocity, flightTime));
            }

            // --- decide which way to point ----------------------------------
            Vector3 desired = bot.BotWander;

            if (Vector3Length(bot.Position) > BOT_LEASH_RADIUS)
            {
                // Too far out. Coming home beats any target.
                desired = Vector3Normalize(Vector3Negate(bot.Position));
                bot.BotWander = desired;
            }
            else if (hasTarget)
            {
                const Vector3 toTarget = Vector3Subtract(aimPoint, bot.Position);
                if (Vector3LengthSqr(toTarget) > 0.0001f)
                    desired = Vector3Normalize(toTarget);
            }
            else if (RandBetween(0.0f, 1.0f) < (float)delta * 0.5f)
            {
                // Nothing about. Drift somewhere new every couple of seconds.
                bot.BotWander = RandomDirection();
                desired = bot.BotWander;
            }

            // Blended rather than replacing, so the bot slides past instead of stopping.
            if (avoiding)
            {
                const Vector3 away = Vector3Subtract(bot.Position, avoidFrom);
                if (Vector3LengthSqr(away) > 0.0001f)
                {
                    const float urgency = 1.0f - (avoidDistance / BOT_AVOID_RANGE);
                    desired = Vector3Normalize(Vector3Lerp(desired, Vector3Normalize(away), urgency));
                }
            }

            // Turns at a fixed rate rather than snapping, so it comes around like a flown ship.
            float alignment = Vector3DotProduct(facing, desired);
            alignment = alignment > 1.0f ? 1.0f : (alignment < -1.0f ? -1.0f : alignment);

            const float angle = acosf(alignment);
            const float step = BOT_TURN_RATE * (float)delta;

            Vector3 heading = desired;
            if (angle > step && angle > 0.0001f)
                heading = Vector3Normalize(Vector3Lerp(facing, desired, step / angle));

            bot.Rotation = RotationFacing(heading);
            bot.Position = Vector3Add(bot.Position,
                                      Vector3Scale(heading, stats.TopSpeed * BOT_SPEED_SCALE * (float)delta));

            // The barrier holds a bot as it holds a player.
            const float botFromCentre = Vector3Length(bot.Position);
            if (botFromCentre > WORLD_RADIUS)
                bot.Position = Vector3Scale(bot.Position, WORLD_RADIUS / botFromCentre);

            // Flying into one still costs, exactly as it does for a player.
            ResolveBotRam(i, now);

            // --- shoot ------------------------------------------------------
            bool aimed = false;
            if (hasTarget && targetDistance < BOT_FIRE_RANGE)
            {
                const Vector3 toTarget = Vector3Subtract(aimPoint, bot.Position);
                if (Vector3LengthSqr(toTarget) > 0.0001f)
                    aimed = Vector3DotProduct(heading, Vector3Normalize(toTarget)) > BOT_AIM_DOT;
            }

            if (aimed && now >= bot.BotNextFireAt && bot.FireTokens >= 1.0f)
            {
                bot.FireTokens -= 1.0f;
                bot.BotNextFireAt = now + RandBetween((float)BOT_FIRE_MIN_GAP, (float)BOT_FIRE_MAX_GAP);

                // The same packet a client sends, so a bot laser draws like anyone else's.
                VolleyPacket volley = {};
                volley.Command = static_cast<int>(NetworkCommands::FireVolley);
                volley.PlayerID = i;
                volley.Position = bot.Position;
                volley.Forward = heading;
                volley.Up = (Vector3){ 0.0f, 1.0f, 0.0f };
                volley.Speed = stats.LaserSpeed;
                volley.Radius = stats.LaserRadius;
                volley.Lifetime = stats.LaserLifetime;
                volley.WeaponId = bot.Upgrades.WeaponEvolution();

                // -1: no client owns this ship, so the volley goes to everyone.
                SendPacketToAllBut(&volley, sizeof(volley), -1);

                // Nobody reports a bot's hits, so they are settled here.
                if (RandBetween(0.0f, 1.0f) < BOT_ACCURACY)
                {
                    if (targetAsteroid != 0)
                    {
                        if (DamageAsteroid(targetAsteroid, stats.Damage))
                        {
                            AwardScore(i, BASE_SCORE);
                            GrantXp(i, BASE_XP);
                            UpdateScoreboard();
                        }
                    }
                    else if (targetPlayer != -1 && CanBeHurt(targetPlayer, now))
                    {
                        ApplyDamage(targetPlayer, LaserDamageToPlayer(i), i, now);
                    }
                }
            }

            // --- tell everyone where it went --------------------------------
            PlayerPacket move = {};
            move.Command = static_cast<int>(NetworkCommands::UpdatePlayer);
            move.Id = i;
            std::strncpy(move.Name, bot.Name, sizeof(move.Name) - 1);
            move.Position = bot.Position;
            move.Rotation = bot.Rotation;
            StampPlayerState(move, i);
            SendPacketToAllBut(&move, sizeof(move), -1);
        }
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

        // Nothing free, but a bot is not worth a person.
        if (playerId == MAX_PLAYERS)
            playerId = EvictOneBot();

        if (playerId < 0 || playerId >= MAX_PLAYERS)
        {
            wsServer.Disconnect(connId);
            return -1;
        }

        players[playerId].Active = true;
        players[playerId].ValidPosition = false;

        // This slot may have just been a bot, and the server would keep flying it.
        players[playerId].IsBot = false;
        players[playerId].ConnId = connId;
        std::memset(players[playerId].Name, 0, sizeof(players[playerId].Name));
        players[playerId].Position = { 0.0f, 0.0f, 0.0f };
        players[playerId].Rotation = MatrixIdentity();
        players[playerId].Score = 0;
        players[playerId].LastInputTime = GetClockSeconds();
        players[playerId].KilledAt = -1.0;

        players[playerId].Upgrades.Reset();
        players[playerId].Health = players[playerId].Upgrades.Stats().MaxHealth;
        players[playerId].LastDamageTime = -1000.0;
        players[playerId].LastHealthReport = -1000.0;
        players[playerId].FireTokens = 0.0f;

        // Seeded from the slot and the clock together.
        players[playerId].OfferRng = (uint32_t)(playerId * 2654435761u)
                                   ^ (uint32_t)(GetClockSeconds() * 1000.0);

        PlayerPacket acceptBuffer = {};
        acceptBuffer.Command = static_cast<int>(NetworkCommands::AcceptPlayer);
        acceptBuffer.Id = playerId;
        SendPacketToOnly(&acceptBuffer, sizeof(acceptBuffer), playerId);

        // Sent after the accept, so the client knows which id the build belongs to before it is handed one.
        SendUpgradeState(playerId);
        SendHealth(playerId, GetClockSeconds());

        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (i == playerId || !players[i].ValidPosition)
                continue;

            PlayerPacket otherBuffer = {};
            otherBuffer.Command = static_cast<int>(NetworkCommands::AddPlayer);
            otherBuffer.Id = i;
            std::memset(otherBuffer.Name, 0, sizeof(otherBuffer.Name));
            if (players[i].Name[0] != 0)
                std::strncpy(otherBuffer.Name, players[i].Name, sizeof(otherBuffer.Name) - 1);
            otherBuffer.Position = players[i].Position;
            otherBuffer.Rotation = players[i].Rotation;
            StampPlayerState(otherBuffer, i);
            SendPacketToOnly(&otherBuffer, sizeof(otherBuffer), playerId);
        }

        return playerId;
    }

    // Named here rather than Names.cpp, which needs raylib the server does not link.
    void MakeBotName(char* out, size_t size, int slot)
    {
        static const char* const kAdjectives[] = {
            "Rusty", "Drifting", "Quiet", "Salvage", "Orbit", "Hollow", "Idle", "Stray"
        };
        static const char* const kNouns[] = {
            "Hauler", "Prospector", "Tug", "Skiff", "Runner", "Lander", "Scow", "Rig"
        };

        const int a = (int)(RandBetween(0.0f, (float)(sizeof(kAdjectives) / sizeof(kAdjectives[0]) - 0.001f)));
        const int n = (int)(RandBetween(0.0f, (float)(sizeof(kNouns) / sizeof(kNouns[0]) - 0.001f)));
        std::snprintf(out, size, "%s%s%02d", kAdjectives[a], kNouns[n], slot % 100);
    }

    // Puts a bot in a player slot. Skips the sends a joining client would get.
    int InitializeNewBot()
    {
        // From the top, so a bot never takes an arriving player's slot.
        const int playerId = AllocateBotSlot();
        if (playerId == -1)
            return -1;

        players[playerId] = ServerPlayer{};
        players[playerId].Active = true;
        players[playerId].IsBot = true;

        // -1 is the no-connection value used everywhere else.
        players[playerId].ConnId = -1;

        // No PlayerPacket arrives for a bot, so its name and position are set here.
        MakeBotName(players[playerId].Name, sizeof(players[playerId].Name), playerId);
        players[playerId].Position = Vector3Scale(RandomDirection(), RandBetween(8.0f, PLAYER_SPAWN_RADIUS));

        // Pointed at the middle on arrival, like a player respawning.
        players[playerId].Rotation = RotationFacing(Vector3Negate(players[playerId].Position));

        // Marks the slot as one the server simulates.
        players[playerId].ValidPosition = true;
        players[playerId].LastInputTime = GetClockSeconds();

        players[playerId].Upgrades.Reset();
        players[playerId].Health = players[playerId].Upgrades.Stats().MaxHealth;
        players[playerId].OfferRng = (uint32_t)(playerId * 2654435761u)
                                   ^ (uint32_t)(GetClockSeconds() * 1000.0);

        // The announcement a human triggers on their first position report.
        PlayerPacket addPacket = {};
        addPacket.Command = static_cast<int>(NetworkCommands::AddPlayer);
        addPacket.Id = playerId;
        std::strncpy(addPacket.Name, players[playerId].Name, sizeof(addPacket.Name) - 1);
        addPacket.Position = players[playerId].Position;
        addPacket.Rotation = players[playerId].Rotation;
        StampPlayerState(addPacket, playerId);
        SendPacketToAllBut(&addPacket, sizeof(addPacket), playerId);

        UpdateScoreboard();
        return playerId;
    }

    void DisconnectPlayer(int playerId)
    {
        if (playerId < 0 || playerId >= MAX_PLAYERS)
            return;

        players[playerId].Active = false;
        players[playerId].ConnId = -1;
        players[playerId].Name[0] = 0;
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
        const double now = GetClockSeconds();

        // The world tracks where people actually are.
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (players[i].Active && players[i].ValidPosition)
                TouchRegionAround(players[i].Position, now);
        }
        PruneRegionCells(now);

        MaintainAsteroidPopulation();

        const int desired = DesiredAsteroidCount();

        for (int i = 0; i < MAX_ASTEROIDS; ++i)
        {
            ServerAsteroid& asteroid = asteroids[i];
            if (!asteroid.Active)
                continue;

            asteroid.Position = Vector3Add(asteroid.Position, Vector3Scale(asteroid.Velocity, (float)delta));

            if (IsInsideRegion(asteroid.Position))
                continue;

            // Outside the world. Leave it be until it is far enough away that moving it cannot be seen happening.
            float distance = DistanceToNearestPlayer(asteroid.Position);
            if (distance >= 0.0f && distance < ASTEROID_RELOCATE_DISTANCE)
                continue;

            // Surplus from splits is dropped here rather than put back.
            if (liveAsteroids > desired)
            {
                ReleaseAsteroid(i);
                continue;
            }

            // Put back somewhere in the world with room around it.
            const int destination = EmptiestRegionCell();
            if (destination < 0)
                continue;

            asteroid.Position = PickPlacementInCell(destination);
            asteroid.Velocity = Vector3Scale(RandomDirection(), RandBetween(1.2f, 3.2f));
        }

        static int networkTickCounter = 0;
        if (++networkTickCounter < ASTEROID_BROADCAST_EVERY)
            return;

        networkTickCounter = 0;
        BroadcastAsteroids();
    }


    void BroadcastAsteroids()
    {
        AsteroidInfoPacket buffer = {};
        buffer.Command = static_cast<int>(NetworkCommands::UpdateAsteroid);

        int count = 0;
        for (int i = 0; i < MAX_ASTEROIDS && count < MAX_ASTEROIDS; ++i)
        {
            if (!asteroids[i].Active)
                continue;

            AsteroidInfo& out = buffer.Asteroids[count++];
            out.Id = asteroids[i].Id;
            out.Seed = asteroids[i].Seed;
            out.Position = asteroids[i].Position;
            out.Velocity = asteroids[i].Velocity;
            out.Scale = asteroids[i].Scale;
            out.Health = asteroids[i].Health;
        }

        buffer.AsteroidCount = count;
        SendPacketToAllBut(&buffer, AsteroidPacketSize(count), -1);
    }

    void HandlePlayerPacket(WebSocketServer::ConnId connId, const PlayerPacket& received)
    {
        int playerId = GetPlayerId(connId);
        if (playerId == -1)
            return;

        if (received.Name[0] != 0)
        {
            std::memset(players[playerId].Name, 0, sizeof(players[playerId].Name));
            std::strncpy(players[playerId].Name, received.Name, sizeof(players[playerId].Name) - 1);
        }

        bool wasFirstUpdate = !players[playerId].ValidPosition;

        // Clamped here too, so a client ignoring the barrier cannot escape the world.
        Vector3 reported = received.Position;
        const float fromCentre = Vector3Length(reported);
        if (fromCentre > WORLD_RADIUS)
            reported = Vector3Scale(reported, WORLD_RADIUS / fromCentre);

        players[playerId].Position = reported;
        players[playerId].Rotation = received.Rotation;
        players[playerId].ValidPosition = true;
        players[playerId].LastInputTime = GetClockSeconds();

        // Announce to everyone else only once we know the player's name and position.
        if (wasFirstUpdate)
        {
            PlayerPacket addPacket = {};
            addPacket.Command = static_cast<int>(NetworkCommands::AddPlayer);
            addPacket.Id = playerId;
            std::strncpy(addPacket.Name, players[playerId].Name, sizeof(addPacket.Name) - 1);
            addPacket.Position = players[playerId].Position;
            addPacket.Rotation = players[playerId].Rotation;
            StampPlayerState(addPacket, playerId);
            SendPacketToAllBut(&addPacket, sizeof(addPacket), playerId);

            UpdateScoreboard();
        }

        PlayerPacket updatePlayerPacket = {};
        updatePlayerPacket.Command = static_cast<int>(NetworkCommands::UpdatePlayer);
        updatePlayerPacket.Id = playerId;
        std::memset(updatePlayerPacket.Name, 0, sizeof(updatePlayerPacket.Name));
        if (players[playerId].Name[0] != 0)
            std::strncpy(updatePlayerPacket.Name, players[playerId].Name, sizeof(updatePlayerPacket.Name) - 1);
        updatePlayerPacket.Position = players[playerId].Position;
        updatePlayerPacket.Rotation = players[playerId].Rotation;
        StampPlayerState(updatePlayerPacket, playerId);
        SendPacketToAllBut(&updatePlayerPacket, sizeof(updatePlayerPacket), playerId);
    }

    // Dispatched on the leading command field; see PeekCommand.
    void OnMessage(WebSocketServer::ConnId connId, const uint8_t* data, size_t len)
    {
        switch (PeekCommand(data, len))
        {
            case NetworkCommands::UpdateInput:
            {
                if (len != sizeof(PlayerPacket))
                    return;

                PlayerPacket received = {};
                memcpy(&received, data, sizeof(PlayerPacket));
                HandlePlayerPacket(connId, received);
                break;
            }

            case NetworkCommands::HitAsteroid:
            {
                if (len != sizeof(AsteroidHitPacket))
                    return;

                AsteroidHitPacket received = {};
                memcpy(&received, data, sizeof(AsteroidHitPacket));

                // The scoring player is taken from the connection rather than the packet.
                int playerId = GetPlayerId(connId);
                if (playerId == -1)
                    return;

                // Read from the shooter's stored build, never from the packet.
                const float damage = players[playerId].Upgrades.Stats().Damage;

                // Only the laser that finishes a rock is paid for it.
                if (DamageAsteroid(received.AsteroidId, damage))
                {
                    AwardScore(playerId, BASE_SCORE);
                    GrantXp(playerId, BASE_XP);
                    UpdateScoreboard();
                }
                break;
            }

            // Still judged by the shooter, as kills used to be.
            case NetworkCommands::PlayerHit:
            {
                if (len != sizeof(PlayerHitPacket))
                    return;

                // The shooter is taken from the connection.
                int shooterId = GetPlayerId(connId);
                if (shooterId == -1)
                    return;

                PlayerHitPacket received = {};
                memcpy(&received, data, sizeof(PlayerHitPacket));

                const int victimId = received.VictimId;
                if (victimId == shooterId)
                    return;

                const double now = GetClockSeconds();
                if (!CanBeHurt(victimId, now))
                    return;

                // Damage is read from the shooter's own stored build rather than from the packet.
                ApplyDamage(victimId, LaserDamageToPlayer(shooterId), shooterId, now);
                break;
            }

            // nobody gains by claiming to have been hurt, and only the victim runs that test.
            case NetworkCommands::AsteroidCollision:
            {
                if (len != sizeof(AsteroidCollisionPacket))
                    return;

                int playerId = GetPlayerId(connId);
                if (playerId == -1)
                    return;

                AsteroidCollisionPacket received = {};
                memcpy(&received, data, sizeof(AsteroidCollisionPacket));

                const int slot = FindAsteroidById(received.AsteroidId);
                if (slot == -1)
                    return;

                // Read before the rock breaks, which frees the slot.
                const float damage = ASTEROID_RAM_DAMAGE * asteroids[slot].Scale;

                // Contact shatters the rock outright.
                if (!BreakAsteroid(received.AsteroidId))
                    return;

                ApplyDamage(playerId, damage, -1, GetClockSeconds());
                break;
            }

            case NetworkCommands::ChooseUpgrade:
            {
                if (len != sizeof(UpgradeChoosePacket))
                    return;

                int playerId = GetPlayerId(connId);
                if (playerId == -1)
                    return;

                UpgradeChoosePacket received = {};
                memcpy(&received, data, sizeof(UpgradeChoosePacket));

                UpgradeState& state = players[playerId].Upgrades;
                const float healthBefore = state.Stats().MaxHealth;

                if (!state.Choose(received.UpgradeId))
                {
                    SendUpgradeState(playerId);
                    break;
                }

                // gives health without full healing
                players[playerId].Health += state.Stats().MaxHealth - healthBefore;
                if (players[playerId].Health > state.Stats().MaxHealth)
                    players[playerId].Health = state.Stats().MaxHealth;
                if (players[playerId].Health < 1.0f)
                    players[playerId].Health = 1.0f;

                SendUpgradeState(playerId);
                SendHealth(playerId, GetClockSeconds());
                break;
            }

            case NetworkCommands::FireVolley:
            {
                if (len != sizeof(VolleyPacket))
                    return;

                int playerId = GetPlayerId(connId);
                if (playerId == -1)
                    return;

                // One credit for the whole pull, however many lasers it becomes.
                if (players[playerId].FireTokens < 1.0f)
                    return;
                players[playerId].FireTokens -= 1.0f;

                VolleyPacket received = {};
                memcpy(&received, data, sizeof(VolleyPacket));

                const ShipStats& stats = players[playerId].Upgrades.Stats();

                // The client says where it stood and faced; every other field is filled in here.
                received.PlayerID = playerId;
                received.Speed = stats.LaserSpeed;
                received.Radius = stats.LaserRadius;
                received.Lifetime = stats.LaserLifetime;
                received.WeaponId = players[playerId].Upgrades.WeaponEvolution();

                SendPacketToAllBut(&received, sizeof(received), playerId);
                break;
            }

            default:
                break;
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
                UpdatePlayers(SERVER_TICK_INTERVAL);
                MaintainBotPopulation();
                UpdateBots(SERVER_TICK_INTERVAL);
                nextTick += SERVER_TICK_INTERVAL;

                // After a long pause the loop would otherwise race to catch up on every missed tick.
                if (now - nextTick > SERVER_TICK_INTERVAL * 5.0)
                    nextTick = now + SERVER_TICK_INTERVAL;
            }

            wsServer.Poll(0,
                [this](WebSocketServer::ConnId connId)
                {
                    int playerId = InitializeNewPlayer(connId);
                    if (playerId != -1)
                    {
                        // The tick maintains the bot population.
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
