#pragma once

#include "networking/NetConstants.h"

#include <cstdint>

// The gun every ship starts with. Every figure in the weapon tree is a multiple
// of these, which is what lets each tier state its whole weapon.
const float BASE_DAMAGE = 100.0f;
const double BASE_FIRE_COOLDOWN = 0.25;
const float BASE_PROJECTILE_SPEED = 30.0f;
const float BASE_PROJECTILE_RADIUS = 0.5f;
const float BASE_PROJECTILE_LIFETIME = 2.0f;

// How a trigger pull is laid out in space. The shape decides where the rounds
// go; the numbers alongside decide how many and how far apart.
enum class PatternShape : uint8_t
{
    // One round, straight down the nose.
    Single = 0,

    // Evenly spread across an arc in the ship's own horizontal plane.
    Fan = 1,

    // Evenly spaced around the forward axis, each angled out by the spread, so
    // the rounds leave as a cone rather than a flat spray.
    Ring = 2,

    // Around the ship rather than ahead of it, spaced evenly around the plane it
    // flies in: ahead, behind, and out to either side.
    Plus = 3,

    // Half the rounds ahead, half behind.
    FrontBack = 4,

    // Two identical groups, one off each side of the hull.
    DualFan = 5,

    // Scattered through a cone instead of around its rim, so a shotgun looks like
    // buckshot. Worked out from the volley and round numbers, so every client agrees.
    Scatter = 6,
};

// When a pattern with two halves swaps between them. Eight evenly spaced rounds
// look the same rotated by half a step; ahead and behind do not.
enum class Alternation : uint8_t
{
    None = 0,

    // The other half comes on the next trigger pull.
    ByVolley = 1,

    // Both halves come from one trigger pull, one burst after the other.
    ByBurst = 2,
};

// The shape and timing of one trigger pull. Numbers rather than a function per
// weapon, because the server works these out too and can check numbers for sense.
struct FirePattern
{
    PatternShape Shape = PatternShape::Single;

    // Rounds in a single burst. A shotgun is ten of these at once; a burst
    // weapon is usually one, repeated.
    int RoundsPerBurst = 1;

    // Total width of the arc for a Fan, or the angle out from the nose for a
    // Ring. Ignored when there is only one round.
    float SpreadRadians = 0.0f;

    // Bursts fired from one trigger pull, and the gap between them. Above one is
    // what makes a weapon fire over time rather than all at once.
    int BurstCount = 1;
    double BurstInterval = 0.0;

    // Multiplied in again for every burst, so a step of 0.5 halves each round
    // after the first and 1.25 grows them.
    float BurstDamageStep = 1.0f;
    float BurstSizeStep = 1.0f;

    // Whether this pattern has a second half, and when it comes. A pattern that
    // alternates has twice as many positions as it fires at once.
    Alternation Alternates = Alternation::None;

    // How far to either side the two groups of a DualFan leave the hull.
    float MuzzleOffset = 0.0f;
};

// A weapon, stated in full. The newest evolution owned sets one outright, so every
// figure is against the base gun. Fire rate is a rate: 2.0 fires twice as often.
struct WeaponProfile
{
    float DamageScale = 1.0f;
    float FireRateScale = 1.0f;
    float ProjectileSpeedScale = 1.0f;
    float ProjectileSizeScale = 1.0f;
    float ProjectileLifetimeScale = 1.0f;

    FirePattern Pattern;

    // Other players are not shown this ship's name.
    bool HidesName = false;

    // Rounds steer gently toward whatever they are nearest to.
    bool Homing = false;
};

// Everything about a ship an upgrade can move. The defaults are the game as it
// played before upgrades existed. Speed is per second; turning is still per frame.
struct ShipStats
{
    float Acceleration = 60.0f;
    float TopSpeed = 9.0f;
    float ReverseTopSpeed = 6.0f;

    double FireCooldown = BASE_FIRE_COOLDOWN;
    float ProjectileSpeed = BASE_PROJECTILE_SPEED;

    // What a shot collides with. Drawing uses a fraction of this, because the
    // hit radius has always been more generous than the dot on screen.
    float ProjectileRadius = BASE_PROJECTILE_RADIUS;

    float ProjectileLifetime = BASE_PROJECTILE_LIFETIME;

    // Equal on purpose: an unupgraded ship kills and dies in one hit, as it always
    // did. Hull upgrades are what make a fight last longer.
    float Damage = BASE_DAMAGE;
    float MaxHealth = 100.0f;

    // How the trigger pull is laid out, and what it carries with it.
    FirePattern Pattern;
    bool HidesName = false;
    bool Homing = false;

    // Rounds fired per trigger pull, across every burst. The rate limit and the
    // projectile pool both need this without wanting to know the shape.
    int RoundsPerTriggerPull = 1;

    // Multiplies everything that hurts us, whoever or whatever sent it. Armour
    // does not ask where a hit came from.
    float DamageTakenScale = 1.0f;

    // Health per second, once nothing has hurt us for REGEN_DELAY_SECONDS.
    float Regen = 0.0f;

    float ScoreMultiplier = 1.0f;

    // How many things a round passes through before it stops. Nothing grants it
    // now, but the firing code still honours it.
    int Pierce = 0;
};

// How long after taking damage before Regen starts giving health back. Long
// enough that it never decides a fight, only the quiet after one.
const double REGEN_DELAY_SECONDS = 4.0;

// Drawn size as a fraction of the collision radius. Shots have always been drawn
// smaller than they hit; this keeps that ratio as rounds grow.
const float PROJECTILE_DRAW_RATIO = 0.2f;

// Every upgrade's wire id. The gaps are retired numbers: an id that has been in a
// pick history must never come back meaning something else.
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

// The exclusive groups the weapon tiers occupy. One upgrade from each may be
// owned, which is what makes the tree a tree rather than a collection.
const uint8_t WEAPON_GROUP_TIER_ONE = 1;
const uint8_t WEAPON_GROUP_TIER_TWO = 2;
const uint8_t WEAPON_GROUP_TIER_THREE = 3;

// One entry in the catalog, and adding an upgrade means adding one of these and
// nothing else. Either a stat card with an Apply or a weapon with a Weapon.
struct UpgradeDef
{
    // Fixed for the life of the upgrade, because it travels on the wire and is
    // stored in pick histories. A retired id is never given to something else.
    uint8_t Id;

    const char* Name;

    // What it does, in numbers rather than prose. The card is small and a player
    // choosing one while being shot at wants the figure, not a sentence about it.
    const char* Description;

    // How many times it can be taken. Evolutions are one-shot; stat cards stack.
    uint8_t MaxRank;

    // Must already be owned before this can be offered. 0 means no prerequisite,
    // which is what makes the evolution tree a tree.
    uint8_t RequiresId;

    // At most one upgrade from a group may ever be owned, so taking a branch
    // closes its siblings for good. 0 means the upgrade excludes nothing.
    uint8_t ExclusiveGroup;

    // The level at which this is offered instead of the ordinary stat cards.
    // 0 means it is an ordinary stat card.
    uint8_t MilestoneLevel;

    // Stat cards only. The whole effect of owning this at the given rank,
    // applied after the weapon profile has been laid down.
    void (*Apply)(ShipStats& stats, int rank);

    // Weapon evolutions only. The gun this branch flies, in full.
    const WeaponProfile* Weapon;
};

namespace UpgradeCatalog
{
    int Count();
    const UpgradeDef& At(int index);

    // Null for an id that is not in the table, which is how a malformed or
    // hostile choice packet gets rejected.
    const UpgradeDef* Find(uint8_t id);

    // True if this level hands out an evolution rather than a stat card.
    bool IsMilestoneLevel(int level);
}

// Total experience needed to reach a level, and the whole pace of the game. Flat,
// so the last level is no further off than the first.
int XpToReachLevel(int level);
int LevelForXp(int xp);

// The most rounds one trigger pull can ever produce. Shottier Gun is thirty and
// Double Barreled twenty, so this has room above the widest weapon in the tree.
#define MAX_VOLLEY_ROUNDS 32

// One round produced by a trigger pull, already placed and aimed.
struct VolleyRound
{
    Vector3 Origin;
    Vector3 Direction;

    // Relative to the ship's damage and round size. Only burst weapons that grow
    // or shrink across the burst move these off 1.
    float DamageScale;
    float SizeScale;

    // Which burst of the pull this belongs to. The caller delays it by this many
    // BurstIntervals rather than firing everything at once.
    int BurstIndex;
};

// Lays out one trigger pull: every round of every burst, placed and aimed. The
// same arguments always give the same rounds, so everyone agrees without asking.
int ExpandVolley(const ShipStats& stats, Vector3 origin, const Matrix& rotation,
                 int volleyIndex, VolleyRound* out, int maxRounds);

// One player's progress and build, stored as the order picks were made rather than
// as ranks. Cutting that order short on death always leaves a legal build.
class UpgradeState
{
public:
    void Reset();

    // Rebuilds Stats from the current history. Cheap enough to call after any
    // change rather than trying to apply upgrades incrementally.
    void Recompute();

    // --- server side ---

    // True when the level changed, which is the signal to roll a fresh offer and
    // send the player their new state.
    bool AddXp(int amount);

    // Halves the level on death and cuts the build down to match. Whatever was
    // picked first survives.
    void ApplyDeathPenalty();

    // Rejects anything that is not in the offer currently on the table, so a
    // client can only ever take one of the cards it was actually shown.
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

    // The furthest weapon evolution owned, and the one byte other players need:
    // which gun to expand a volley from, and whether to draw this ship's name.
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
