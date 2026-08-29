#include "include/ServerHost.h"

#include "include/WebSocketServer.h"
#include "include/networking/NetConstants.h"
#include "include/Upgrades.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <random>
#include <string>
#include <thread>

#define BASE_SCORE 5

// A player is a harder target than a rock, so worth more.
#define KILL_SCORE 25

// Not the same number as score. Score is the leaderboard and resets on death;
// experience is the build, and dying only costs half of it.
#define BASE_XP 5
#define KILL_XP 25

// For testing; defaults to the real game so an ordinary build cannot ship it.
//   make server EXTRA_SERVER_DEFINES=-DXP_MULTIPLIER=20
#ifndef XP_MULTIPLIER
#define XP_MULTIPLIER 1
#endif

// What a full-sized rock does to a health it runs into. An unupgraded ship has a
// hundred, so a big asteroid still ends a run exactly as it always did.
#define ASTEROID_RAM_DAMAGE 130.0f

// How many trigger pulls of credit a ship can bank. Timing the gap exactly would
// throw a laser away every time a frame ran late.
const float FIRE_BURST_VOLLEYS = 2.0f;

// How often a regenerating health tells its owner about it. Regen is gentle and the
// bar moves slowly, so there is no reason to spend a packet every tick.
const double HEALTH_REPORT_INTERVAL = 0.2;

const double SERVER_TICK_RATE = 30.0;
const double SERVER_TICK_INTERVAL = 1.0 / SERVER_TICK_RATE;

// Asteroid positions are sent every Nth tick, and clients keep the rocks moving in
// between. A bigger number saves traffic but lets their guess drift further.
const int ASTEROID_BROADCAST_EVERY = 3; // 30Hz / 3 = 10Hz

// Cap on asteroids created in a single tick, so filling an empty field is spread
// over a few frames rather than arriving as one spike.
const int ASTEROID_SPAWNS_PER_TICK = 4;

// Applies a bounty, rounded rather than truncated. Truncating ate the first rank
// on cheap targets: five plus fifteen per cent is 5.75, cut back to five.
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

    // The build and the health it produces, both here rather than on the client: one
    // that owned its own build could award itself anything, and never die.
    UpgradeState Upgrades;
    float Health = 100.0f;

    // Seeded per player, so two people levelling in the same instant are not
    // handed the same three cards.
    uint32_t OfferRng = 0;

    // When they last reported in, and when they were last killed. Together these
    // stop a ship whose client has stopped running from being farmed for points.
    double LastInputTime = 0.0;
    double KilledAt = -1.0;

    // When something last took health off them, which is what a repair bay waits
    // on before it starts giving any back.
    double LastDamageTime = -1000.0;

    // Credit for firing, in lasers. Spent one per laser and refilled at whatever
    // rate the build on file is entitled to.
    float FireTokens = 0.0f;

    // When their own health was last reported to them.
    double LastHealthReport = -1000.0;
};

// One switched-on cube of the play area.
struct RegionCell
{
    int X = 0, Y = 0, Z = 0;
    double LastOccupied = 0.0;
};

// The server's own record of an asteroid, separate from the version sent to
// clients because it holds things they never need.
struct ServerAsteroid
{
    bool Active = false;
    uint32_t Id = 0;
    uint8_t Seed = 0;
    Vector3 Position = { 0.0f, 0.0f, 0.0f };
    Vector3 Velocity = { 0.0f, 0.0f, 0.0f };
    float Scale = 1.0f;

    // Worked out from Scale whenever the rock is made or resized, so its size and
    // its toughness cannot drift apart.
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

    // Never reused, so a client can tell an asteroid that moved apart from a
    // different one that took over its old slot.
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

    // Throws away points outside the sphere and tries again, which spreads the
    // directions evenly. Scaling each axis instead bunches them at the corners.
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

    // Players actually in the world, not just ones with a socket open. Counting a
    // player who has not sent a position yet would leave rocks nowhere to go.
    int ActivePlayerCount() const
    {
        int count = 0;
        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (players[i].Active && players[i].ValidPosition)
                count++;
        }
        return count;
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

    // Switches on the eight cubes meeting at the nearest grid corner, so a player
    // near an edge does not end up with world on one side and nothing on the other.
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

    // Looks for a spot in the cube with room around it, so an asteroid is never
    // put back into play on top of somebody.
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
        // Scales with how much world is switched on, not with how many players
        // are connected, so the field feels the same however far people spread.
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

    // Tops the field up, always filling whichever cube has fewest, so somebody who
    // has just arrived somewhere new is not left in an empty sky.
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

    // Takes a bite out of a rock, breaking it only once nothing is left. Returns
    // true for the laser that finished it, which is what decides who is paid.
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

        // A guard here used to ignore repeat reports within a tenth of a second.
        // Reports are single lasers now, and a shotgun lands ten in one frame.
        ServerAsteroid parent = asteroids[slot];

        // Freed before the fragments are allocated, so a split never has to
        // compete with its own parent for a slot.
        ReleaseAsteroid(slot);

        float childScale = parent.Scale * ASTEROID_SPLIT_FACTOR;
        if (childScale < MIN_ASTEROID_SCALE)
            return true; // Too small to be worth splitting: it just shatters.

        // The two pieces are pushed out sideways from the direction the rock was
        // going, so they visibly fly apart instead of following its old path.
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

    // Everything below stamps or derives from server-held state. None of it
    // trusts a number that arrived from a client.

    // The fields a client may not describe about itself. Called on every outbound
    // PlayerPacket, so a health or a level reaches nobody except from here.
    void StampPlayerState(PlayerPacket& packet, int playerId)
    {
        packet.Health = players[playerId].Health;
        packet.MaxHealth = players[playerId].Upgrades.Stats().MaxHealth;
        packet.Level = (uint8_t)players[playerId].Upgrades.Level();
        packet.Evolution = players[playerId].Upgrades.WeaponEvolution();
    }

    void SendHealth(int playerId, double now)
    {
        players[playerId].LastHealthReport = now;

        PlayerHealthPacket buffer = {};
        buffer.Command = static_cast<int>(NetworkCommands::UpdateHealth);
        buffer.Health = players[playerId].Health;
        buffer.MaxHealth = players[playerId].Upgrades.Stats().MaxHealth;
        SendPacketToOnly(&buffer, sizeof(buffer), playerId);
    }

    // Rolls a fresh offer first if one is owed, so a client is never told a pick
    // is waiting without also being told what the pick is between.
    void SendUpgradeState(int playerId)
    {
        UpgradeState& state = players[playerId].Upgrades;

        if (state.PendingPicks() > 0 && state.OfferCount() == 0)
            state.RollOffer(players[playerId].OfferRng);

        UpgradeStatePacket buffer = {};
        state.WriteTo(buffer);
        SendPacketToOnly(&buffer, sizeof(buffer), playerId);
    }

    // All experience is awarded here, so one multiplier retunes the pace. Bounty
    // applies to it as well as score, and so compounds into more picks.
    void GrantXp(int playerId, int amount)
    {
        const float multiplier = players[playerId].Upgrades.Stats().ScoreMultiplier;

        // Levelling only ever hands out a pick. Nothing about the ship changes
        // until the player actually chooses something.
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

        // Dying costs the whole score but only half the build, so a good run
        // still leaves something behind to fly back out on.
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

        // The victim is told so it can respawn; it had no way to know, since the
        // laser was judged on the shooter's screen.
        PlayerKillPacket notify = {};
        notify.Command = static_cast<int>(NetworkCommands::PlayerKilled);
        notify.KillerId = killerId;
        notify.VictimId = victimId;
        SendPacketToOnly(&notify, sizeof(notify), victimId);

        SendUpgradeState(victimId);
        SendHealth(victimId, now);
        UpdateScoreboard();
    }

    // Every point of damage arrives here, which is why armour is applied here
    // rather than at the call sites. It does not care what the damage was.
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

    // Stops an abandoned ship being farmed: a client not running frames never
    // processes its respawn, so it sits there as a free target.
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

    // Health comes back only after a stretch with nothing hitting us, so a repair
    // bay decides what happens between fights rather than during one.
    void UpdatePlayers(double delta)
    {
        const double now = GetClockSeconds();

        for (int i = 0; i < MAX_PLAYERS; ++i)
        {
            if (!players[i].Active || !players[i].ValidPosition)
                continue;

            const ShipStats& stats = players[i].Upgrades.Stats();

            // Credit builds up even while nobody is firing, which is what lets a
            // pull leave immediately after a pause.
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
        players[playerId].LastInputTime = GetClockSeconds();
        players[playerId].KilledAt = -1.0;

        players[playerId].Upgrades.Reset();
        players[playerId].Health = players[playerId].Upgrades.Stats().MaxHealth;
        players[playerId].LastDamageTime = -1000.0;
        players[playerId].LastHealthReport = -1000.0;
        players[playerId].FireTokens = 0.0f;

        // Seeded from the slot and the clock together, so two people joining in
        // the same moment do not walk the same sequence of offers.
        players[playerId].OfferRng = (uint32_t)(playerId * 2654435761u)
                                   ^ (uint32_t)(GetClockSeconds() * 1000.0);

        PlayerPacket acceptBuffer = {};
        acceptBuffer.Command = static_cast<int>(NetworkCommands::AcceptPlayer);
        acceptBuffer.Id = playerId;
        SendPacketToOnly(&acceptBuffer, sizeof(acceptBuffer), playerId);

        // Sent after the accept, so the client knows which id the build belongs
        // to before it is handed one.
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

        // The world tracks where people actually are: a cube switches on under
        // each player and switches off once it has been empty for a while.
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

            // Outside the world. Leave it be until it is far enough away that
            // moving it cannot be seen happening.
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
            asteroid.Position = PickPlacementInCell(EmptiestRegionCell());
            asteroid.Velocity = Vector3Scale(RandomDirection(), RandBetween(1.2f, 3.2f));
        }

        static int networkTickCounter = 0;
        if (++networkTickCounter < ASTEROID_BROADCAST_EVERY)
            return;

        networkTickCounter = 0;
        BroadcastAsteroids();
    }

    // Only asteroids that actually exist are sent, and the packet is cut down to
    // fit just those; see AsteroidPacketSize.
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

        players[playerId].Position = received.Position;
        players[playerId].Rotation = received.Rotation;
        players[playerId].ValidPosition = true;
        players[playerId].LastInputTime = GetClockSeconds();

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

                // The scoring player is taken from the connection rather than the
                // packet, so a client cannot bank points into someone else's slot.
                int playerId = GetPlayerId(connId);
                if (playerId == -1)
                    return;

                // Read from the shooter's stored build, never from the packet, so
                // a client cannot decide for itself how hard its guns hit.
                const float damage = players[playerId].Upgrades.Stats().Damage;

                // Only the laser that finishes a rock is paid for it, so score
                // still counts rocks rather than trigger pulls.
                if (DamageAsteroid(received.AsteroidId, damage))
                {
                    AwardScore(playerId, BASE_SCORE);
                    GrantXp(playerId, BASE_XP);
                    UpdateScoreboard();
                }
                break;
            }

            // Still judged by the shooter, as kills used to be. Landing one now
            // takes health off rather than ending a run on its own.
            case NetworkCommands::PlayerHit:
            {
                if (len != sizeof(PlayerHitPacket))
                    return;

                // The shooter is taken from the connection, so a client can only
                // ever report its own lasers, never someone else's.
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

                // Damage is read from the shooter's own stored build rather than
                // from the packet, so a client cannot inflate what its guns do.
                ApplyDamage(victimId, players[shooterId].Upgrades.Stats().Damage, shooterId, now);
                break;
            }

            // Self-reported, and safe to be: nobody gains by claiming to have been
            // hurt, and only the victim runs that test.
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

                // Read before the rock breaks, which frees the slot. Armour is
                // left to ApplyDamage, or it would count twice.
                const float damage = ASTEROID_RAM_DAMAGE * asteroids[slot].Scale;

                // Contact shatters the rock outright, or a ship tough enough to
                // survive would sit inside it taking the hit every frame.
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

                // Refused unless it was one of the cards actually shown. A refusal
                // still answers, so a drifted client is corrected rather than stuck.
                if (!state.Choose(received.UpgradeId))
                {
                    SendUpgradeState(playerId);
                    break;
                }

                // New plating arrives as plating, not as a repair: it should help
                // mid-fight without undoing the fight.
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

                // The client says where it stood and faced; every other field is
                // filled in here, so nobody can claim a gun they have not earned.
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
                nextTick += SERVER_TICK_INTERVAL;

                // After a long pause the loop would otherwise race to catch up
                // on every missed tick. Skip them and carry on from now.
                if (now - nextTick > SERVER_TICK_INTERVAL * 5.0)
                    nextTick = now + SERVER_TICK_INTERVAL;
            }

            wsServer.Poll(0,
                [this](WebSocketServer::ConnId connId)
                {
                    int playerId = InitializeNewPlayer(connId);
                    if (playerId != -1)
                        UpdateScoreboard();
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
