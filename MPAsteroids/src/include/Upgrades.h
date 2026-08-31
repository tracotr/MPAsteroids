#pragma once

#include "networking/NetConstants.h"

#include <cstdint>

// The gun every ship starts with.
const float BASE_DAMAGE = 100.0f;
const double BASE_FIRE_COOLDOWN = 0.25;
const float BASE_LASER_SPEED = 30.0f;
const float BASE_LASER_RADIUS = 0.5f;
const float BASE_LASER_LIFETIME = 2.0f;

// How a trigger pull is laid out in space.
enum class PatternShape : uint8_t
{
    // One laser, straight down the nose.
    Single = 0,

    // Evenly spread across an arc in the ship's own horizontal plane.
    Fan = 1,

    // Evenly spaced around the forward axis, each angled out by the spread.
    Ring = 2,

    // Around the ship rather than ahead of it, spaced evenly around the plane it flies in.
    Plus = 3,

    // Half the lasers ahead, half behind.
    FrontBack = 4,

    // Two identical groups, one off each side of the health.
    DualFan = 5,

    // Scattered through a cone instead of around its rim, so a shotgun looks like buckshot.
    Scatter = 6,
};

// When a pattern with two halves swaps between them.
enum class Alternation : uint8_t
{
    None = 0,

    // The other half comes on the next trigger pull.
    ByVolley = 1,

    // Both halves come from one trigger pull, one burst after the other.
    ByBurst = 2,
};

// The shape and timing of one trigger pull.
struct FirePattern
{
    PatternShape Shape = PatternShape::Single;

    // Lasers in a single burst. A shotgun is ten of these at once; a burst weapon is usually one, repeated.
    int LasersPerBurst = 1;

    // Total width of the arc for a Fan, or the angle out from the nose for a Ring.
    float SpreadRadians = 0.0f;

    // Bursts fired from one trigger pull, and the gap between them.
    int BurstCount = 1;
    double BurstInterval = 0.0;

    // Multiplied in again for every burst.
    float BurstDamageStep = 1.0f;
    float BurstSizeStep = 1.0f;

    // Whether this pattern has a second half, and when it comes.
    Alternation Alternates = Alternation::None;

    // How far to either side the two groups of a DualFan leave the health.
    float MuzzleOffset = 0.0f;
};

// A weapon, stated in full. The newest evolution owned sets one outright.
struct WeaponProfile
{
    float DamageScale = 1.0f;
    float FireRateScale = 1.0f;
    float LaserSpeedScale = 1.0f;
    float LaserSizeScale = 1.0f;
    float LaserLifetimeScale = 1.0f;

    FirePattern Pattern;

    // Other players are not shown this ship's name.
    bool HidesName = false;

    // Lasers steer gently toward whatever they are nearest to.
    bool Homing = false;
};

// Everything about a ship an upgrade can move.
struct ShipStats
{
    float Acceleration = 60.0f;
    float TopSpeed = 9.0f;
    float ReverseTopSpeed = 6.0f;

    double FireCooldown = BASE_FIRE_COOLDOWN;
    float LaserSpeed = BASE_LASER_SPEED;

    // What a laser collides with.
    float LaserRadius = BASE_LASER_RADIUS;

    float LaserLifetime = BASE_LASER_LIFETIME;

    // Equal on purpose: an unupgraded ship kills and dies in one hit, as it always
    // did. Health upgrades are what make a fight last longer.
    float Damage = BASE_DAMAGE;
    float MaxHealth = 100.0f;

    // How the trigger pull is laid out, and what it carries with it.
    FirePattern Pattern;
    bool HidesName = false;
    bool Homing = false;

    // Lasers fired per trigger pull, across every burst.
    int LasersPerTriggerPull = 1;

    // Multiplies everything that hurts us, whoever or whatever sent it.
    float DamageTakenScale = 1.0f;

    // Health per second, once nothing has hurt us for REGEN_DELAY_SECONDS.
    float Regen = 0.0f;

    float ScoreMultiplier = 1.0f;

    // How many things a laser passes through before it stops.
    int Pierce = 0;
};

// How long after taking damage before Regen starts giving health back.
const double REGEN_DELAY_SECONDS = 4.0;

// Drawn size as a fraction of the collision radius.
const float LASER_DRAW_RATIO = 0.2f;

// Every upgrade's wire id. The gaps are retired numbers.
enum UpgradeId : uint8_t
{
    UPGRADE_NONE = 0,

    UPGRADE_THRUSTERS = 1,
    UPGRADE_RAPID_FIRE = 2,
    UPGRADE_HIGH_VELOCITY = 3,
    UPGRADE_HEAVY_ROUNDS = 4,
    UPGRADE_LONG_BARREL = 5,
    UPGRADE_HULL_PLATING = 6,
    UPGRADE_BOUNTY = 7,
    UPGRADE_ARMOR = 8,
    UPGRADE_REPAIR_BAY = 9,

    // Tier one, at level 10.
    UPGRADE_TWIN = 70,
    UPGRADE_SNIPER = 71,
    UPGRADE_MACHINE_GUN = 72,

    // Tier two, at level 20.
    UPGRADE_TRIPLE = 80,
    UPGRADE_QUAD = 81,
    UPGRADE_ASSASSIN = 82,
    UPGRADE_DOUBLE_TAP = 83,
    UPGRADE_BIG_BOY = 84,
    UPGRADE_SHOTGUN = 85,

    // Tier three, at level 30.
    UPGRADE_TRIPLE_DIPPER = 90,
    UPGRADE_QUINTUPLE_DIPPER = 91,
    UPGRADE_NORTH_STAR = 92,
    UPGRADE_SWITCHER = 93,
    UPGRADE_EVIL_BEING = 94,
    UPGRADE_BIG_BEING = 95,
    UPGRADE_TRIPLE_TAP = 96,
    UPGRADE_QUINTUPLE_TAP = 97,
    UPGRADE_BIGGEST_BOY = 98,
    UPGRADE_DOUBLE_DIPPER = 99,
    UPGRADE_SHOTTIER_GUN = 100,
    UPGRADE_DOUBLE_BARRELED = 101,
};

// The exclusive groups the weapon tiers occupy.
const uint8_t WEAPON_GROUP_TIER_ONE = 1;
const uint8_t WEAPON_GROUP_TIER_TWO = 2;
const uint8_t WEAPON_GROUP_TIER_THREE = 3;

// One entry in the catalog, and adding an upgrade means adding one of these and nothing else.
struct UpgradeDef
{
    // Fixed for the life of the upgrade, because it travels on the wire and is stored in pick histories.
    uint8_t Id;

    const char* Name;

    // What it does, in numbers rather than prose.
    const char* Description;

    // How many times it can be taken. Evolutions are taken once; cards stack.
    uint8_t MaxRank;

    // Must already be owned before this can be offered. 0 means no prerequisite.
    uint8_t RequiresId;

    // At most one upgrade from a group may ever be owned.
    uint8_t ExclusiveGroup;

    // The level at which this is offered instead of the ordinary stat cards. 0 means it is an ordinary stat card.
    uint8_t MilestoneLevel;

    // Stat cards only. The whole effect of owning this at the given rank.
    void (*Apply)(ShipStats& stats, int rank);

    // Weapon evolutions only. The gun this branch flies, in full.
    const WeaponProfile* Weapon;
};

namespace UpgradeCatalog
{
    int Count();
    const UpgradeDef& At(int index);

    // Null for an id that is not in the table.
    const UpgradeDef* Find(uint8_t id);

    // True if this level hands out an evolution rather than a stat card.
    bool IsMilestoneLevel(int level);
}

// Total experience needed to reach a level, and the whole pace of the game.
int XpToReachLevel(int level);
int LevelForXp(int xp);

// The most lasers one trigger pull can ever produce.
#define MAX_VOLLEY_LASERS 32

// One laser produced by a trigger pull, already placed and aimed.
struct VolleyLaser
{
    Vector3 Origin;
    Vector3 Direction;

    // Relative to the ship's damage and laser size.
    float DamageScale;
    float SizeScale;

    // Which burst of the pull this belongs to.
    int BurstIndex;
};

// Lays out one trigger pull: every laser of every burst, placed and aimed.
int ExpandVolley(const ShipStats& stats, Vector3 origin, const Matrix& rotation,
                 int volleyIndex, VolleyLaser* out, int maxRounds);

// One player's progress and build, stored as the order picks were made rather than as ranks.
class UpgradeState
{
public:
    void Reset();

    // Rebuilds Stats from the current history.
    void Recompute();

    // --- server side ---

    // True when the level changed, which is the signal to roll a fresh offer and send the player their new state.
    bool AddXp(int amount);

    // Halves the level on death and cuts the build down to match.
    void ApplyDeathPenalty();

    // Rejects anything that is not in the offer currently on the table.
    bool Choose(uint8_t upgradeId);

    void RollOffer(uint32_t& rngState);

    // --- both sides ---

    const ShipStats& Stats() const { return stats; }
    int Level() const { return level; }
    int Xp() const { return xp; }
    int PendingPicks() const { return level - historyCount; }
    int OfferCount() const { return offerCount; }
    uint8_t OfferedId(int index) const;
    int RankOf(uint8_t upgradeId) const;

    // The furthest weapon evolution owned, and the one byte other players need.
    uint8_t WeaponEvolution() const;

    // --- wire ---

    void WriteTo(UpgradeStatePacket& packet) const;
    void ReadFrom(const UpgradeStatePacket& packet);

private:
    bool IsEligible(const UpgradeDef& def) const;

    int xp = 0;
    int level = 0;

    uint8_t offered[UPGRADE_OFFER_COUNT] = { 0 };
    int offerCount = 0;

    uint8_t history[MAX_UPGRADE_PICKS] = { 0 };
    int historyCount = 0;

    ShipStats stats;
};
