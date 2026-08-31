#include "include/Upgrades.h"

#include <cmath>
#include <cstring>

namespace
{
    // Angles are written in degrees 
    constexpr float Deg(float degrees) { return degrees * 0.01745329252f; }

    // A burst weapon fires its lasers this far apart unless it says otherwise.
    constexpr double BURST_GAP = 0.07;

    // The weapons. Every figure is against the base gun, not the tier below, because a tier replaces it.

    // --- tier one ---------------------------------------------------------

    const WeaponProfile TwinProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 0.75f;
        w.FireRateScale = 1.0f;
        w.Pattern.Shape = PatternShape::Fan;
        w.Pattern.LasersPerBurst = 2;
        w.Pattern.SpreadRadians = Deg(3.0f);
        return w;
    }();


    const WeaponProfile SniperProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 2.0f;
        w.FireRateScale = 0.4f;
        w.LaserSpeedScale = 1.8f;
        return w;
    }();

    const WeaponProfile MachineGunProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 0.9f;
        w.FireRateScale = 2.0f;
        return w;
    }();

    // --- tier two, from Twin ----------------------------------------------

    const WeaponProfile TripleProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 0.75f;
        w.FireRateScale = 1.0f;
        w.Pattern.Shape = PatternShape::Fan;
        w.Pattern.LasersPerBurst = 3;
        w.Pattern.SpreadRadians = Deg(14.0f);
        return w;
    }();


    const WeaponProfile QuadProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 1.2f;
        w.Pattern.Shape = PatternShape::Plus;
        w.Pattern.LasersPerBurst = 4;
        return w;
    }();

    // --- tier two, from Sniper --------------------------------------------

    const WeaponProfile AssassinProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 2.5f;
        w.FireRateScale = 0.4f;
        w.LaserSpeedScale = 2.0f;
        w.HidesName = true;
        return w;
    }();

    const WeaponProfile DoubleTapProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 1.75f;
        w.FireRateScale = 0.75f;
        w.LaserSpeedScale = 1.5f;
        w.Pattern.BurstCount = 2;
        w.Pattern.BurstInterval = BURST_GAP;
        return w;
    }();

    // --- tier two, from Machine Gun ---------------------------------------

    const WeaponProfile BigBoyProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 3.0f;
        w.LaserSpeedScale = 0.5f;
        w.LaserSizeScale = 5.0f;
        return w;
    }();

    const WeaponProfile ShotgunProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 0.2f;
        w.FireRateScale = 0.8f;
        w.LaserSpeedScale = 1.3f;
        w.LaserLifetimeScale = 0.3f;
        w.Pattern.Shape = PatternShape::Scatter;
        w.Pattern.LasersPerBurst = 10;
        w.Pattern.SpreadRadians = Deg(12.0f);
        return w;
    }();

    // --- tier three, from Triple ------------------------------------------

    const WeaponProfile TripleDipperProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 0.75f;
        w.FireRateScale = 1.5f;
        w.Pattern.Shape = PatternShape::Fan;
        w.Pattern.LasersPerBurst = 3;
        w.Pattern.SpreadRadians = Deg(6.0f);
        return w;
    }();

    const WeaponProfile QuintupleDipperProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 0.9f;
        w.FireRateScale = 1.2f;
        w.Pattern.Shape = PatternShape::Fan;
        w.Pattern.LasersPerBurst = 5;
        w.Pattern.SpreadRadians = Deg(22.0f);
        return w;
    }();

    // --- tier three, from Quad --------------------------------------------

    // Sixteen positions around the ship, eight fired at a time.
    const WeaponProfile NorthStarProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 1.3f;
        w.FireRateScale = 1.25f;
        w.Pattern.Shape = PatternShape::Plus;
        w.Pattern.LasersPerBurst = 8;

        // Eight lasers a pull, every other position
        w.Pattern.Alternates = Alternation::ByVolley;
        return w;
    }();


    const WeaponProfile SwitcherProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 1.2f;
        w.FireRateScale = 1.3f;
        w.Pattern.Shape = PatternShape::FrontBack;
        w.Pattern.LasersPerBurst = 4;
        w.Pattern.SpreadRadians = Deg(20.0f);

        // Ahead on one pull, behind on the next
        w.Pattern.Alternates = Alternation::ByVolley;
        return w;
    }();

    // --- tier three, from Assassin ----------------------------------------

    const WeaponProfile EvilBeingProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 4.0f;
        w.FireRateScale = 0.4f;
        w.LaserSpeedScale = 4.0f;
        w.HidesName = true;
        return w;
    }();

    const WeaponProfile BigBeingProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 3.0f;
        w.FireRateScale = 0.4f;
        w.LaserSpeedScale = 3.0f;
        w.Homing = true;
        return w;
    }();

    // --- tier three, from Double-tap --------------------------------------

    const WeaponProfile TripleTapProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 1.65f;
        w.FireRateScale = 0.9f;
        w.LaserSpeedScale = 1.75f;
        w.Pattern.BurstCount = 3;
        w.Pattern.BurstInterval = BURST_GAP;
        return w;
    }();

    const WeaponProfile QuintupleTapProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 1.3f;
        w.FireRateScale = 0.5f;
        w.LaserSpeedScale = 1.75f;
        w.Pattern.BurstCount = 5;
        w.Pattern.BurstInterval = BURST_GAP;
        w.Pattern.BurstSizeStep = 1.25f;
        return w;
    }();

    // --- tier three, from Big Boy -----------------------------------------

    const WeaponProfile BiggestBoyProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 5.0f;
        w.LaserSpeedScale = 0.5f;
        w.LaserSizeScale = 7.5f;
        return w;
    }();

    // A Big Boy laser, then a second at half the damage and half the size.
    const WeaponProfile DoubleDipperProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 3.0f;
        w.LaserSpeedScale = 0.5f;
        w.LaserSizeScale = 5.0f;
        w.Pattern.BurstCount = 2;
        w.Pattern.BurstInterval = BURST_GAP;
        w.Pattern.BurstDamageStep = 0.5f;
        w.Pattern.BurstSizeStep = 0.5f;
        return w;
    }();

    // --- tier three, from Shotgun -----------------------------------------

    const WeaponProfile ShottierGunProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 0.1f;
        w.FireRateScale = 0.5f;
        w.LaserSpeedScale = 1.2f;
        w.LaserLifetimeScale = 0.3f;
        w.Pattern.Shape = PatternShape::Scatter;
        w.Pattern.LasersPerBurst = 30;
        w.Pattern.SpreadRadians = Deg(20.0f);
        return w;
    }();

    // A shotgun off each side of the health at once.
    const WeaponProfile DoubleBarreledProfile = []
    {
        WeaponProfile w;
        w.DamageScale = 0.2f;
        w.FireRateScale = 0.8f;
        w.LaserSpeedScale = 1.3f;
        w.LaserLifetimeScale = 0.3f;
        w.Pattern.Shape = PatternShape::DualFan;
        w.Pattern.LasersPerBurst = 20;
        w.Pattern.SpreadRadians = Deg(12.0f);
        w.Pattern.MuzzleOffset = 1.2f;
        return w;
    }();

    // The catalog, and adding an upgrade means adding a row here and nothing else.
    const UpgradeDef Catalog[] =
    {
        // --- stat cards, offered at every ordinary level ---

        { UPGRADE_THRUSTERS, "Agile Thrusters", "+12% accel, +8% top speed",
          5, 0, 0, 0,
          [](ShipStats& s, int rank)
          {
              s.Acceleration *= 1.0f + 0.12f * rank;
              s.TopSpeed *= 1.0f + 0.08f * rank;
          }, nullptr },

        { UPGRADE_RAPID_FIRE, "Rapid Fire", "-12% fire delay",
          5, 0, 0, 0,
          [](ShipStats& s, int rank) { s.FireCooldown *= powf(0.88f, (float)rank); }, nullptr },

        { UPGRADE_HIGH_VELOCITY, "High Velocity", "+15% laser speed",
          5, 0, 0, 0,
          [](ShipStats& s, int rank) { s.LaserSpeed *= 1.0f + 0.15f * rank; }, nullptr },

        { UPGRADE_HEAVY_ROUNDS, "Heavy Rounds", "+18% size, +10% damage",
          5, 0, 0, 0,
          [](ShipStats& s, int rank)
          {
              s.LaserRadius *= 1.0f + 0.18f * rank;
              s.Damage *= 1.0f + 0.10f * rank;
          }, nullptr },

        { UPGRADE_LONG_BARREL, "Long Barrel", "+20% range",
          5, 0, 0, 0,
          [](ShipStats& s, int rank) { s.LaserLifetime *= 1.0f + 0.20f * rank; }, nullptr },

        { UPGRADE_HULL_PLATING, "Hull Plating", "+25 health",
          5, 0, 0, 0,
          [](ShipStats& s, int rank) { s.MaxHealth += 25.0f * rank; }, nullptr },

        { UPGRADE_BOUNTY, "Bounty", "+15% score and xp",
          5, 0, 0, 0,
          [](ShipStats& s, int rank) { s.ScoreMultiplier += 0.15f * rank; }, nullptr },

        { UPGRADE_ARMOR, "Ablative Armor", "-30% damage taken",
          3, 0, 0, 0,
          [](ShipStats& s, int rank) { s.DamageTakenScale *= powf(0.7f, (float)rank); }, nullptr },

        { UPGRADE_REPAIR_BAY, "Repair Bay", "+1.5 health/sec",
          3, 0, 0, 0,
          [](ShipStats& s, int rank) { s.Regen += 1.5f * rank; }, nullptr },

        // --- tier one, level 10 ---

        { UPGRADE_TWIN, "Twin", "2 lasers, 1x rate, 0.75x dmg",
          1, 0, WEAPON_GROUP_TIER_ONE, 10, nullptr, &TwinProfile },

        { UPGRADE_SNIPER, "Sniper", "2x dmg, 0.4x rate, 1.8x speed",
          1, 0, WEAPON_GROUP_TIER_ONE, 10, nullptr, &SniperProfile },

        { UPGRADE_MACHINE_GUN, "Machine Gun", "2x rate, 0.9x dmg",
          1, 0, WEAPON_GROUP_TIER_ONE, 10, nullptr, &MachineGunProfile },

        // --- tier two, level 20 ---

        { UPGRADE_TRIPLE, "Triple Shot", "3 lasers in an arc, 1x rate, 0.75x dmg",
          1, UPGRADE_TWIN, WEAPON_GROUP_TIER_TWO, 20, nullptr, &TripleProfile },

        { UPGRADE_QUAD, "Quad", "4 lasers around you, 1.2x dmg",
          1, UPGRADE_TWIN, WEAPON_GROUP_TIER_TWO, 20, nullptr, &QuadProfile },

        { UPGRADE_ASSASSIN, "Assassin", "2.5x dmg, 0.4x rate, 2x speed, name hidden",
          1, UPGRADE_SNIPER, WEAPON_GROUP_TIER_TWO, 20, nullptr, &AssassinProfile },

        { UPGRADE_DOUBLE_TAP, "Double-tap", "2-laser burst, 1.75x dmg, 0.75x rate",
          1, UPGRADE_SNIPER, WEAPON_GROUP_TIER_TWO, 20, nullptr, &DoubleTapProfile },

        { UPGRADE_BIG_BOY, "Big Boy", "5x size, 0.5x speed, 3x dmg",
          1, UPGRADE_MACHINE_GUN, WEAPON_GROUP_TIER_TWO, 20, nullptr, &BigBoyProfile },

        { UPGRADE_SHOTGUN, "Shotgun", "10 lasers, 0.2x dmg each, 1.3x speed",
          1, UPGRADE_MACHINE_GUN, WEAPON_GROUP_TIER_TWO, 20, nullptr, &ShotgunProfile },

        // --- tier three, level 30 ---

        { UPGRADE_TRIPLE_DIPPER, "Triple Dipper", "3 lasers, tight arc, 1.5x rate, 0.75x dmg",
          1, UPGRADE_TRIPLE, WEAPON_GROUP_TIER_THREE, 30, nullptr, &TripleDipperProfile },

        { UPGRADE_QUINTUPLE_DIPPER, "Quintuple Dipper", "5 lasers spread, 1.2x rate, 0.9x dmg",
          1, UPGRADE_TRIPLE, WEAPON_GROUP_TIER_THREE, 30, nullptr, &QuintupleDipperProfile },

        { UPGRADE_NORTH_STAR, "North Star", "8 of 16 lasers around you, 1.3x dmg",
          1, UPGRADE_QUAD, WEAPON_GROUP_TIER_THREE, 30, nullptr, &NorthStarProfile },

        { UPGRADE_SWITCHER, "Switcher", "4 lasers ahead, then 4 behind, 1.2x dmg",
          1, UPGRADE_QUAD, WEAPON_GROUP_TIER_THREE, 30, nullptr, &SwitcherProfile },

        { UPGRADE_EVIL_BEING, "Evil Being", "4x dmg, 0.4x rate, 4x speed, name hidden",
          1, UPGRADE_ASSASSIN, WEAPON_GROUP_TIER_THREE, 30, nullptr, &EvilBeingProfile },

        { UPGRADE_BIG_BEING, "Big Being", "3x dmg, 0.4x rate, 3x speed, lasers track",
          1, UPGRADE_ASSASSIN, WEAPON_GROUP_TIER_THREE, 30, nullptr, &BigBeingProfile },

        { UPGRADE_TRIPLE_TAP, "Triple Tap", "3-laser burst, 1.65x dmg, 1.75x speed",
          1, UPGRADE_DOUBLE_TAP, WEAPON_GROUP_TIER_THREE, 30, nullptr, &TripleTapProfile },

        { UPGRADE_QUINTUPLE_TAP, "Quintuple Tap", "5-laser burst, growing, 1.3x dmg",
          1, UPGRADE_DOUBLE_TAP, WEAPON_GROUP_TIER_THREE, 30, nullptr, &QuintupleTapProfile },

        { UPGRADE_BIGGEST_BOY, "Biggest Boy", "7.5x size, 0.5x speed, 5x dmg",
          1, UPGRADE_BIG_BOY, WEAPON_GROUP_TIER_THREE, 30, nullptr, &BiggestBoyProfile },

        { UPGRADE_DOUBLE_DIPPER, "Double Dipper Big Bipper", "2-laser burst, second at half",
          1, UPGRADE_BIG_BOY, WEAPON_GROUP_TIER_THREE, 30, nullptr, &DoubleDipperProfile },

        { UPGRADE_SHOTTIER_GUN, "Shottier Gun", "30 lasers, 0.1x dmg each, 0.5x rate",
          1, UPGRADE_SHOTGUN, WEAPON_GROUP_TIER_THREE, 30, nullptr, &ShottierGunProfile },

        { UPGRADE_DOUBLE_BARRELED, "Double Barreled", "a shotgun off each side, 20 lasers",
          1, UPGRADE_SHOTGUN, WEAPON_GROUP_TIER_THREE, 30, nullptr, &DoubleBarreledProfile },
    };

    const int CatalogCount = (int)(sizeof(Catalog) / sizeof(Catalog[0]));

    // Rolled here rather than with the standard library.
    uint32_t NextRandom(uint32_t& state)
    {
        if (state == 0) state = 0x9E3779B9u;
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    // Folds a weapon into the fields the rest of the game reads.
    void ApplyWeaponProfile(ShipStats& stats, const WeaponProfile& weapon)
    {
        stats.Damage = BASE_DAMAGE * weapon.DamageScale;
        stats.LaserSpeed = BASE_LASER_SPEED * weapon.LaserSpeedScale;
        stats.LaserRadius = BASE_LASER_RADIUS * weapon.LaserSizeScale;
        stats.LaserLifetime = BASE_LASER_LIFETIME * weapon.LaserLifetimeScale;

        const float rate = (weapon.FireRateScale > 0.0001f) ? weapon.FireRateScale : 0.0001f;
        stats.FireCooldown = BASE_FIRE_COOLDOWN / rate;

        stats.Pattern = weapon.Pattern;
        stats.HidesName = weapon.HidesName;
        stats.Homing = weapon.Homing;
    }
}

namespace
{
    // A direction on the disc facing the way the ship does.
    Vector3 RadialDirection(const Vector3& right, const Vector3& up, float theta)
    {
        return Vector3Add(Vector3Scale(right, cosf(theta)), Vector3Scale(up, sinf(theta)));
    }

    // A direction on the disc the ship flies in, the one you would see looking down on it.
    Vector3 HorizontalDirection(const Vector3& forward, const Vector3& right, float theta)
    {
        return Vector3Add(Vector3Scale(forward, cosf(theta)), Vector3Scale(right, sinf(theta)));
    }

    // Tilts a radial direction back toward the nose by the spread.
    Vector3 ConeDirection(const Vector3& forward, const Vector3& radial, float spread)
    {
        return Vector3Normalize(Vector3Add(Vector3Scale(forward, cosf(spread)),
                                           Vector3Scale(radial, sinf(spread))));
    }

    // A settled value in nought to one, from the volley and laser numbers rather than a generator.
    float ScatterUnit(int volleyIndex, int roundIndex, uint32_t salt)
    {
        uint32_t h = (uint32_t)volleyIndex * 73856093u
                   ^ (uint32_t)roundIndex * 19349663u
                   ^ salt * 83492791u;
        h ^= h >> 13;
        h *= 0x5bd1e995u;
        h ^= h >> 15;
        return (float)(h & 0xFFFFFFu) / (float)0xFFFFFFu;
    }

    // A laser somewhere inside the cone.
    Vector3 ScatterDirection(const Vector3& forward, const Vector3& right, const Vector3& up,
                             int volleyIndex, int roundIndex, float spread)
    {
        const float radius = spread * sqrtf(ScatterUnit(volleyIndex, roundIndex, 1u));
        const float theta = 2.0f * PI * ScatterUnit(volleyIndex, roundIndex, 2u);

        return Vector3Normalize(Vector3Add(Vector3Scale(forward, cosf(radius)),
                                           Vector3Scale(Vector3Add(Vector3Scale(right, cosf(theta)),
                                                                   Vector3Scale(up, sinf(theta))),
                                                        sinf(radius))));
    }

    // Evenly spaced across an arc, centred on whatever it is given.
    Vector3 FanDirection(const Vector3& axis, const Vector3& centre, int index, int count, float spread)
    {
        if (count <= 1)
            return centre;

        const float angle = ((float)index / (float)(count - 1) - 0.5f) * spread;
        return Vector3Transform(centre, MatrixRotate(axis, angle));
    }
}

int ExpandVolley(const ShipStats& stats, Vector3 origin, const Matrix& rotation,
                 int volleyIndex, VolleyLaser* out, int maxRounds)
{
    const FirePattern& pattern = stats.Pattern;

    const Vector3 forward = Vector3Normalize(Vector3Transform((Vector3){ 0.0f, 0.0f, -1.0f }, rotation));
    const Vector3 up = Vector3Normalize(Vector3Transform((Vector3){ 0.0f, 1.0f, 0.0f }, rotation));
    const Vector3 right = Vector3Normalize(Vector3Transform((Vector3){ 1.0f, 0.0f, 0.0f }, rotation));

    const int perBurst = pattern.LasersPerBurst > 0 ? pattern.LasersPerBurst : 1;
    const bool alternates = (pattern.Alternates != Alternation::None);

    int count = 0;
    float damageScale = 1.0f;
    float sizeScale = 1.0f;

    for (int burst = 0; burst < pattern.BurstCount; burst++)
    {
        // Worked out per burst rather than once.
        const bool flipped =
            (pattern.Alternates == Alternation::ByVolley && ((volleyIndex & 1) != 0)) ||
            (pattern.Alternates == Alternation::ByBurst && ((burst & 1) != 0));

        for (int i = 0; i < perBurst && count < maxRounds; i++)
        {
            VolleyLaser& laser = out[count++];
            laser.Origin = origin;
            laser.Direction = forward;
            laser.DamageScale = damageScale;
            laser.SizeScale = sizeScale;
            laser.BurstIndex = burst;

            switch (pattern.Shape)
            {
                case PatternShape::Single:
                    break;

                case PatternShape::Fan:
                    laser.Direction = FanDirection(up, forward, i, perBurst, pattern.SpreadRadians);
                    break;

                case PatternShape::Scatter:
                    laser.Direction = ScatterDirection(forward, right, up, volleyIndex, i,
                                                       pattern.SpreadRadians);
                    break;

                case PatternShape::Ring:
                {
                    const float theta = 2.0f * PI * (float)i / (float)perBurst;
                    laser.Direction = ConeDirection(forward, RadialDirection(right, up, theta),
                                                    pattern.SpreadRadians);
                    break;
                }

                case PatternShape::Plus:
                {
                    // Out from the health, around the plane the ship flies in, first laser down the nose.
                    const int slots = alternates ? perBurst * 2 : perBurst;
                    const int slot = alternates ? (i * 2 + (flipped ? 1 : 0)) : i;
                    const float theta = 2.0f * PI * (float)slot / (float)slots;
                    laser.Direction = Vector3Normalize(HorizontalDirection(forward, right, theta));
                    break;
                }

                case PatternShape::FrontBack:
                {
                    const Vector3 centre = flipped ? Vector3Negate(forward) : forward;
                    laser.Direction = FanDirection(up, centre, i, perBurst, pattern.SpreadRadians);
                    break;
                }

                case PatternShape::DualFan:
                {
                    // Two identical groups, one off each side of the health.
                    const int half = perBurst > 1 ? perBurst / 2 : 1;
                    const bool leftSide = (i >= half);
                    const int inGroup = leftSide ? (i - half) : i;

                    laser.Origin = Vector3Add(origin,
                                              Vector3Scale(right, leftSide ? -pattern.MuzzleOffset
                                                                           : pattern.MuzzleOffset));

                    // Each barrel scatters independently.
                    laser.Direction = ScatterDirection(forward, right, up, volleyIndex,
                                                       inGroup + (leftSide ? 512 : 0),
                                                       pattern.SpreadRadians);
                    break;
                }
            }
        }

        damageScale *= pattern.BurstDamageStep;
        sizeScale *= pattern.BurstSizeStep;
    }

    return count;
}

namespace UpgradeCatalog
{
    int Count() { return CatalogCount; }

    const UpgradeDef& At(int index) { return Catalog[index]; }

    const UpgradeDef* Find(uint8_t id)
    {
        for (int i = 0; i < CatalogCount; i++)
        {
            if (Catalog[i].Id == id)
                return &Catalog[i];
        }
        return nullptr;
    }

    bool IsMilestoneLevel(int level)
    {
        for (int i = 0; i < CatalogCount; i++)
        {
            if (Catalog[i].MilestoneLevel == level)
                return true;
        }
        return false;
    }
}

// Every level costs the same.
const int XP_PER_LEVEL = 60;

int XpToReachLevel(int level)
{
    if (level <= 0) return 0;
    if (level > MAX_LEVEL) level = MAX_LEVEL;
    return XP_PER_LEVEL * level;
}

int LevelForXp(int xp)
{
    int level = 0;
    while (level < MAX_LEVEL && xp >= XpToReachLevel(level + 1))
        level++;
    return level;
}

void UpgradeState::Reset()
{
    xp = 0;
    level = 0;
    offerCount = 0;
    historyCount = 0;
    std::memset(offered, 0, sizeof(offered));
    std::memset(history, 0, sizeof(history));
    Recompute();
}

int UpgradeState::RankOf(uint8_t upgradeId) const
{
    int rank = 0;
    for (int i = 0; i < historyCount; i++)
    {
        if (history[i] == upgradeId)
            rank++;
    }
    return rank;
}

uint8_t UpgradeState::OfferedId(int index) const
{
    if (index < 0 || index >= offerCount)
        return UPGRADE_NONE;
    return offered[index];
}

// Rebuilt in two passes: the newest evolution lays the gun down.
void UpgradeState::Recompute()
{
    stats = ShipStats();

    const WeaponProfile* weapon = nullptr;
    for (int i = 0; i < CatalogCount; i++)
    {
        if (Catalog[i].Weapon != nullptr && RankOf(Catalog[i].Id) > 0)
            weapon = Catalog[i].Weapon;
    }

    if (weapon != nullptr)
        ApplyWeaponProfile(stats, *weapon);

    for (int i = 0; i < CatalogCount; i++)
    {
        if (Catalog[i].Apply == nullptr)
            continue;

        const int rank = RankOf(Catalog[i].Id);
        if (rank > 0)
            Catalog[i].Apply(stats, rank);
    }

    if (stats.Pattern.LasersPerBurst < 1) stats.Pattern.LasersPerBurst = 1;
    if (stats.Pattern.BurstCount < 1) stats.Pattern.BurstCount = 1;
    stats.LasersPerTriggerPull = stats.Pattern.LasersPerBurst * stats.Pattern.BurstCount;

    if (stats.MaxHealth < 1.0f) stats.MaxHealth = 1.0f;
    if (stats.FireCooldown < 0.02) stats.FireCooldown = 0.02;
}

uint8_t UpgradeState::WeaponEvolution() const
{
    uint8_t found = UPGRADE_NONE;

    // Table order runs tier one before two before three.
    for (int i = 0; i < CatalogCount; i++)
    {
        if (Catalog[i].Weapon != nullptr && RankOf(Catalog[i].Id) > 0)
            found = Catalog[i].Id;
    }

    return found;
}

bool UpgradeState::IsEligible(const UpgradeDef& def) const
{
    if (RankOf(def.Id) >= def.MaxRank)
        return false;

    if (def.RequiresId != UPGRADE_NONE && RankOf(def.RequiresId) == 0)
        return false;

    // Taking one member of a group shuts the others for good.
    if (def.ExclusiveGroup != 0)
    {
        for (int i = 0; i < CatalogCount; i++)
        {
            if (Catalog[i].Id == def.Id) continue;
            if (Catalog[i].ExclusiveGroup != def.ExclusiveGroup) continue;
            if (RankOf(Catalog[i].Id) > 0)
                return false;
        }
    }

    return true;
}

bool UpgradeState::AddXp(int amount)
{
    if (amount <= 0)
        return false;

    const int cap = XpToReachLevel(MAX_LEVEL);
    xp += amount;
    if (xp > cap) xp = cap;

    const int newLevel = LevelForXp(xp);
    if (newLevel == level)
        return false;

    level = newLevel;
    return true;
}

void UpgradeState::ApplyDeathPenalty()
{
    const int newLevel = level / 2;

    if (historyCount > newLevel)
        historyCount = newLevel;

    level = newLevel;
    xp = XpToReachLevel(newLevel);
    offerCount = 0;

    Recompute();
}


void UpgradeState::RollOffer(uint32_t& rngState)
{
    offerCount = 0;
    std::memset(offered, 0, sizeof(offered));

    if (PendingPicks() <= 0 || historyCount >= MAX_UPGRADE_PICKS)
        return;

    const int pickLevel = historyCount + 1;
    const bool isMilestone = UpgradeCatalog::IsMilestoneLevel(pickLevel);

    uint8_t candidates[64];
    int candidateCount = 0;


    bool fromMilestoneList = false;
    // A milestone offers evolutions only.
    for (int pass = 0; pass < 2 && candidateCount == 0; pass++)
    {
        const bool wantMilestone = isMilestone && (pass == 0);

        for (int i = 0; i < CatalogCount && candidateCount < (int)sizeof(candidates); i++)
        {
            const UpgradeDef& def = Catalog[i];

            if (wantMilestone)
            {
                if (def.MilestoneLevel != pickLevel) continue;
            }
            else
            {
                if (def.MilestoneLevel != 0) continue;
            }

            if (!IsEligible(def)) continue;
            candidates[candidateCount++] = def.Id;
        }

        if (candidateCount > 0)
            fromMilestoneList = wantMilestone;

        // An ordinary level has nothing to fall back to, so one pass is all it ever needs.
        if (!isMilestone)
            break;
    }

    const int wanted = candidateCount < UPGRADE_OFFER_COUNT ? candidateCount : UPGRADE_OFFER_COUNT;

    // A branch choice is never shuffled, so key is always the same
    if (fromMilestoneList)
    {
        for (int i = 0; i < wanted; i++)
            offered[offerCount++] = candidates[i];
        return;
    }

    // ordinary draft
    for (int i = 0; i < wanted; i++)
    {
        const int j = i + (int)(NextRandom(rngState) % (uint32_t)(candidateCount - i));
        const uint8_t swap = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = swap;

        offered[offerCount++] = candidates[i];
    }
}

bool UpgradeState::Choose(uint8_t upgradeId)
{
    if (PendingPicks() <= 0 || historyCount >= MAX_UPGRADE_PICKS)
        return false;

    // Only ever one of the cards actually shown.
    bool wasOffered = false;
    for (int i = 0; i < offerCount; i++)
    {
        if (offered[i] == upgradeId)
        {
            wasOffered = true;
            break;
        }
    }

    if (!wasOffered)
        return false;

    const UpgradeDef* def = UpgradeCatalog::Find(upgradeId);
    if (def == nullptr || !IsEligible(*def))
        return false;

    history[historyCount++] = upgradeId;
    offerCount = 0;
    std::memset(offered, 0, sizeof(offered));

    Recompute();
    return true;
}

void UpgradeState::WriteTo(UpgradeStatePacket& packet) const
{
    packet.Command = NetworkCommands::UpdateUpgrades;
    packet.Xp = xp;
    packet.Level = level;
    packet.PendingPicks = PendingPicks();
    packet.HistoryCount = historyCount;
    packet.OfferCount = (uint8_t)offerCount;

    std::memset(packet.Offered, 0, sizeof(packet.Offered));
    for (int i = 0; i < offerCount; i++)
        packet.Offered[i] = offered[i];

    std::memset(packet.History, 0, sizeof(packet.History));
    for (int i = 0; i < historyCount; i++)
        packet.History[i] = history[i];

    std::memset(packet.Padding, 0, sizeof(packet.Padding));
}

// Everything here arrives from the server, but is still clamped.
void UpgradeState::ReadFrom(const UpgradeStatePacket& packet)
{
    xp = packet.Xp;
    level = packet.Level;
    if (level < 0) level = 0;
    if (level > MAX_LEVEL) level = MAX_LEVEL;

    historyCount = packet.HistoryCount;
    if (historyCount < 0) historyCount = 0;
    if (historyCount > MAX_UPGRADE_PICKS) historyCount = MAX_UPGRADE_PICKS;
    if (historyCount > level) historyCount = level;

    std::memset(history, 0, sizeof(history));
    for (int i = 0; i < historyCount; i++)
        history[i] = packet.History[i];

    offerCount = packet.OfferCount;
    if (offerCount < 0) offerCount = 0;
    if (offerCount > UPGRADE_OFFER_COUNT) offerCount = UPGRADE_OFFER_COUNT;

    std::memset(offered, 0, sizeof(offered));
    for (int i = 0; i < offerCount; i++)
        offered[i] = packet.Offered[i];

    Recompute();
}
