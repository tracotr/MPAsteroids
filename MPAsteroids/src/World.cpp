#include "include/World.h"
#include "include/GameApp.h"
#include "include/UpgradeUI.h"

World* World::Instance = nullptr;

World& World::Create()
{
    if (!Instance)
    {
        Instance = new World();
    }
    
    return *Instance;
}

void World::Destroy()
{
	if (Instance)
		delete(Instance);
	Instance = nullptr;
}

void World::Reset()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    PlayerShip.Reset();
}

void World::Update(double delta)
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

    PlayerShip.Update(delta);
    
    if (PlayerShip.isFiring) {
        FireVolley(PlayerShip.Position, PlayerShip.Rotation, Net.GetStats(), Net.GetLocalPlayerId());
    }

    // Volleys that arrived since the last frame, laid out here rather than sent
    // round by round.
    for (int i = 0; i < Net.RemoteVolleyCount; i++)
        SpawnRemoteVolley(Net.RemoteVolleyQueue[i], (float)Net.QueuedVolleyAge(i));
    Net.RemoteVolleyCount = 0;
    
    // Before the projectiles move, so a burst round that came due this frame
    // travels the same distance as one fired at the top of it.
    UpdatePendingRounds(GetTime());

    UpdateProjectiles(delta);
    
    Net.UpdateLocalPlayer(PlayerShip.Position, PlayerShip.Rotation);
    Net.NetUpdate(GetTime(), delta);

    // The shooter decides hits, so being killed arrives as a message rather
    // than something we detect ourselves.
    int killerId = -1;
    if (Net.ConsumeKilled(&killerId))
    {
        Vector3 hitPosition = PlayerShip.Position;
        PlayerShip.Respawn();
        Sounds::PlayHurt(hitPosition, PlayerShip.Position);
    }

    // Read after the network update, so a card that arrived this frame can be
    // taken this frame rather than on the next one.
    UpgradeUI::Update(Net);

    RefreshAsteroidFrame();
    CheckCollisions();
}

void World::Draw()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();

    Models::DrawSkybox();
    PlayerShip.Draw();
    DrawPlayerModels();
    DrawAsteroidModels();
    DrawProjectiles();
}

void World::DrawUI()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

    Models::DrawUI(PlayerShip.Position, Net.GetLocalPlayerId(), Net.GetScoreboard(), Net.GetPlayerNames());

    auto& playerNames = Net.GetPlayerNames();
    PlayerIndicator otherPlayersData[MAX_PLAYERS];
    int otherPlayerCount = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (i == Net.GetLocalPlayerId()) continue;

        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();

        if (Net.GetPlayerSpatial(i, &pos, &rot))
        {
            otherPlayersData[otherPlayerCount].position = pos;
            otherPlayersData[otherPlayerCount].name = playerNames[i];

            float health = 0.0f;
            float maxHealth = -1.0f;
            Net.GetPlayerHealth(i, &health, &maxHealth);
            otherPlayersData[otherPlayerCount].health = health;
            otherPlayersData[otherPlayerCount].maxHealth = maxHealth;
            otherPlayersData[otherPlayerCount].level = Net.GetPlayerLevel(i);

            // Read from the weapon the server stamps on their updates, so a
            // client cannot hide its own name simply by claiming to.
            const uint8_t evolution = Net.GetPlayerEvolution(i);
            const UpgradeDef* weapon = (evolution != UPGRADE_NONE) ? UpgradeCatalog::Find(evolution) : nullptr;
            otherPlayersData[otherPlayerCount].nameHidden =
                (weapon != nullptr && weapon->Weapon != nullptr && weapon->Weapon->HidesName);

            otherPlayerCount++;
        }
    }

    DrawPlayerIndicators(otherPlayersData, otherPlayerCount, GetScreenWidth(), GetScreenHeight());

    UpgradeUI::Draw(Net);
}


void World::DrawPlayerIndicators(const PlayerIndicator* otherPlayersData, int otherPlayerCount, int screenWidth, int screenHeight) {
    Camera3D camera = GameApp::GetInstance()->GetCamera();

    Vector2 screenCenter = { (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
    float edgeMargin = 40.0f;

    for (int i = 0; i < otherPlayerCount; i++) {
        Vector3 targetPos = otherPlayersData[i].position;

        const char* playerName = otherPlayersData[i].name;
        const bool showName = !otherPlayersData[i].nameHidden;

        float distance = Vector3Distance(camera.position, targetPos);
        if (distance > PLAYER_INDICATOR_RANGE)
            continue;

        Vector2 screenPos = GetWorldToScreen(targetPos, camera);

        Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        Vector3 toTarget = Vector3Subtract(targetPos, camera.position);
        bool isBehind = Vector3DotProduct(forward, toTarget) < 0.0f;

        if (isBehind) {
            screenPos.x = screenCenter.x - (screenPos.x - screenCenter.x);
            screenPos.y = screenCenter.y - (screenPos.y - screenCenter.y);
        }

        bool isOffScreen = isBehind || 
                           screenPos.x < edgeMargin || 
                           screenPos.x > (screenWidth - edgeMargin) || 
                           screenPos.y < edgeMargin || 
                           screenPos.y > (screenHeight - edgeMargin);

        if (isOffScreen) {
            Vector2 dir = Vector2Normalize(Vector2Subtract(screenPos, screenCenter));
            Vector2 clampedPos = {
                Clamp(screenCenter.x + dir.x * (screenWidth / 2.0f - edgeMargin), edgeMargin, screenWidth - edgeMargin),
                Clamp(screenCenter.y + dir.y * (screenHeight / 2.0f - edgeMargin), edgeMargin, screenHeight - edgeMargin)
            };

            float angle = atan2f(dir.y, dir.x);
            Vector2 p1 = { clampedPos.x + cosf(angle) * 12.0f, clampedPos.y + sinf(angle) * 12.0f };
            Vector2 p2 = { clampedPos.x + cosf(angle + 2.4f) * 10.0f, clampedPos.y + sinf(angle + 2.4f) * 10.0f };
            Vector2 p3 = { clampedPos.x + cosf(angle - 2.4f) * 10.0f, clampedPos.y + sinf(angle - 2.4f) * 10.0f };

            DrawTriangle(p1, p3, p2, RED);
            
            if (showName)
                DrawText(playerName, (int)(clampedPos.x - 15), (int)(clampedPos.y + 12), 10, RAYWHITE);
            DrawText(TextFormat("%0.0fm", distance), (int)(clampedPos.x - 15), (int)(clampedPos.y + 22), 10, LIGHTGRAY);
        } else {            
            if (showName)
                DrawText(playerName, (int)(screenPos.x - 15), (int)(screenPos.y - 38), 10, RAYWHITE);
            DrawText(TextFormat("%0.0fm  L%i", distance, otherPlayersData[i].level),
                     (int)(screenPos.x - 15), (int)(screenPos.y - 28), 10, LIGHTGRAY);

            // Only over someone with enough plating to survive more than one
            // shot; a full bar over every ship would say the same thing each time.
            const float maxHealth = otherPlayersData[i].maxHealth;
            if (maxHealth > 100.0f)
            {
                const float fraction = Clamp(otherPlayersData[i].health / maxHealth, 0.0f, 1.0f);
                const int barX = (int)(screenPos.x - 15);
                const int barY = (int)(screenPos.y - 16);

                DrawRectangle(barX, barY, 40, 4, (Color){ 0, 0, 0, 170 });
                DrawRectangle(barX, barY, (int)(40 * fraction), 4, (Color){ 210, 90, 90, 230 });
            }
        }
    }
}

// Bends a tracking round toward whatever is worth hitting near it. Ships first,
// or a round that locked onto the nearest boulder would never reach anybody.
void World::SteerHomingRound(Projectile& round, double delta)
{
    NetClient& Net = GameApp::GetInstance()->GetNet();

    const float speed = Vector3Length(round.velocity);
    if (speed < 0.0001f)
        return;

    const Vector3 heading = Vector3Scale(round.velocity, 1.0f / speed);

    Vector3 target = { 0.0f, 0.0f, 0.0f };
    float bestDistanceSq = PROJECTILE_HOMING_RANGE * PROJECTILE_HOMING_RANGE;
    bool found = false;

    // Only things ahead of the round are worth turning toward; anything behind it
    // is already missed, and chasing it would look like a guided missile.
    auto consider = [&](Vector3 position)
    {
        const Vector3 toTarget = Vector3Subtract(position, round.position);
        const float distanceSq = Vector3LengthSqr(toTarget);

        if (distanceSq > bestDistanceSq || distanceSq < 0.0001f)
            return;

        if (Vector3DotProduct(Vector3Normalize(toTarget), heading) <= 0.0f)
            return;

        bestDistanceSq = distanceSq;
        target = position;
        found = true;
    };

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (i == round.ownerId)
            continue;

        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Matrix rotation = MatrixIdentity();
        if (!Net.GetPlayerSpatial(i, &position, &rotation))
            continue;

        consider(position);
    }

    // Rocks only if nobody is worth chasing. The list is one frame old, which at
    // these speeds is not worth reordering the frame over.
    if (!found)
    {
        for (int i = 0; i < AsteroidFrameCount; i++)
            consider(AsteroidFrames[i].Position);
    }

    if (!found)
        return;

    const Vector3 desired = Vector3Normalize(Vector3Subtract(target, round.position));

    float dot = Vector3DotProduct(heading, desired);
    if (dot > 1.0f) dot = 1.0f;
    if (dot < -1.0f) dot = -1.0f;

    const float angle = acosf(dot);
    const float maxTurn = PROJECTILE_HOMING_TURN_RATE * (float)delta;

    if (angle <= maxTurn)
    {
        round.velocity = Vector3Scale(desired, speed);
        return;
    }

    Vector3 axis = Vector3CrossProduct(heading, desired);
    if (Vector3LengthSqr(axis) < 0.000001f)
        return;

    axis = Vector3Normalize(axis);
    round.velocity = Vector3Scale(Vector3Transform(heading, MatrixRotate(axis, maxTurn)), speed);
}

void World::UpdateProjectiles(double delta)
{
    int highest = 0;

    for (int i = 0; i < ProjectileHighWater; i++)
    {
        if (!Projectiles[i].active)
            continue;

        // Steered before it moves, so the round travels this frame along the
        // heading it just turned onto rather than the one it had last frame.
        if (Projectiles[i].homing)
            SteerHomingRound(Projectiles[i], delta);

        Projectiles[i].previousPosition = Projectiles[i].position;
        Projectiles[i].position = Vector3Add(Projectiles[i].position, Vector3Scale(Projectiles[i].velocity, delta));
        Projectiles[i].lifeTime -= delta;

        if (Projectiles[i].lifeTime <= 0.0f)
            Projectiles[i].active = false;
        else
            highest = i + 1;
    }

    // Pulled back in as rounds expire, so a quiet moment after a shotgun volley
    // costs what a quiet moment should.
    ProjectileHighWater = highest;
}

void World::FireProjectile(Vector3 position, Vector3 velocity, int ownerId, float lifeTime,
                           float radius, int pierce, float damage, bool homing)
{
    NetClient& Net = GameApp::GetInstance()->GetNet();

    // Our own rounds may take any slot; everyone else's start past the reserve.
    // A lobby firing shotguns fills this pool fast, and our shot must not be lost.
    const bool isLocal = (ownerId == Net.GetLocalPlayerId());
    const int firstSlot = isLocal ? 0 : LOCAL_PROJECTILE_RESERVE;

    for (int i = firstSlot; i < MAX_PROJECTILES; i++)
    {
        if (!Projectiles[i].active)
        {
            Projectiles[i].active = true;
            Projectiles[i].ownerId = ownerId;
            Projectiles[i].position = position;
            Projectiles[i].previousPosition = position;
            Projectiles[i].velocity = velocity;
            Projectiles[i].lifeTime = lifeTime;
            Projectiles[i].radius = radius;
            Projectiles[i].pierceLeft = pierce;
            Projectiles[i].damage = damage;
            Projectiles[i].homing = homing;

            if (i + 1 > ProjectileHighWater)
                ProjectileHighWater = i + 1;
            break;
        }
    }
}

// Where the rounds go is decided by the shared pattern code, so everyone lays a
// volley out the same way. This only turns that layout into projectiles.
void World::FireVolley(Vector3 origin, const Matrix& rotation, const ShipStats& stats, int ownerId)
{
    NetClient& Net = GameApp::GetInstance()->GetNet();

    VolleyRound rounds[MAX_VOLLEY_ROUNDS];
    const int count = ExpandVolley(stats, origin, rotation, LocalVolleyIndex, rounds, MAX_VOLLEY_ROUNDS);

    // One message for the whole pull. Everyone receiving it walks the same pattern
    // to the same rounds, so thirty pellets cost what one used to.
    Net.SendVolley(origin,
                   Vector3Normalize(Vector3Transform((Vector3){ 0.0f, 0.0f, -1.0f }, rotation)),
                   Vector3Normalize(Vector3Transform((Vector3){ 0.0f, 1.0f, 0.0f }, rotation)),
                   LocalVolleyIndex);

    // Counted per pull, not per round: an alternating weapon swaps halves once a
    // trigger pull, however many rounds that pull turned out to be.
    LocalVolleyIndex++;

    const double now = GetTime();

    for (int i = 0; i < count; i++)
    {
        const VolleyRound& round = rounds[i];

        const Vector3 velocity = Vector3Scale(round.Direction, stats.ProjectileSpeed);
        const float radius = stats.ProjectileRadius * round.SizeScale;
        const float damage = stats.Damage * round.DamageScale;

        // The first burst leaves now; the rest wait their turn.
        if (round.BurstIndex == 0)
            ReleaseRound(round.Origin, velocity, ownerId, stats.ProjectileLifetime, radius,
                         stats.Pierce, damage, stats.Homing);
        else
            QueueRound(now + round.BurstIndex * stats.Pattern.BurstInterval,
                       round.Origin, velocity, ownerId, stats.ProjectileLifetime, radius,
                       stats.Pierce, damage, stats.Homing);
    }
}

// Fires one round. Nothing is sent from here: the whole pull was described once
// when the trigger went down, so this is purely a local event.
void World::ReleaseRound(Vector3 origin, Vector3 velocity, int ownerId, float lifeTime,
                         float radius, int pierce, float damage, bool homing)
{
    FireProjectile(origin, velocity, ownerId, lifeTime, radius, pierce, damage, homing);
}

// Rebuilds a rotation from the two axes a volley carries. The model's own
// forward is -Z, which is why the third axis is the negation of it.
static Matrix RotationFromAxes(Vector3 forward, Vector3 up)
{
    Vector3 zAxis = Vector3Negate(Vector3Normalize(forward));
    Vector3 upHint = Vector3Normalize(up);

    Vector3 xAxis = Vector3CrossProduct(upHint, zAxis);
    if (Vector3LengthSqr(xAxis) < 0.000001f)
        return MatrixIdentity();

    xAxis = Vector3Normalize(xAxis);
    Vector3 yAxis = Vector3CrossProduct(zAxis, xAxis);

    Matrix result = MatrixIdentity();
    result.m0 = xAxis.x; result.m1 = xAxis.y; result.m2 = xAxis.z;
    result.m4 = yAxis.x; result.m5 = yAxis.y; result.m6 = yAxis.z;
    result.m8 = zAxis.x; result.m9 = zAxis.y; result.m10 = zAxis.z;
    return result;
}

// Turns somebody else's trigger pull back into rounds. Age is how long the packet
// took to arrive: a burst already due is caught up, one still ahead waits.
void World::SpawnRemoteVolley(const NetClient::RemoteVolleyEvent& volley, float age)
{
    const UpgradeDef* def = (volley.WeaponId != UPGRADE_NONE)
                          ? UpgradeCatalog::Find(volley.WeaponId) : nullptr;

    // Only the pattern and tracking come from the weapon. The rest was stamped by
    // the server from that player's whole build, stat cards and all.
    ShipStats stats;
    if (def != nullptr && def->Weapon != nullptr)
    {
        stats.Pattern = def->Weapon->Pattern;
        stats.Homing = def->Weapon->Homing;
    }

    const Matrix rotation = RotationFromAxes(volley.Forward, volley.Up);

    VolleyRound rounds[MAX_VOLLEY_ROUNDS];
    const int count = ExpandVolley(stats, volley.Position, rotation, volley.VolleyIndex,
                                   rounds, MAX_VOLLEY_ROUNDS);

    const double now = GetTime();

    for (int i = 0; i < count; i++)
    {
        const VolleyRound& round = rounds[i];

        const Vector3 velocity = Vector3Scale(round.Direction, volley.Speed);
        const float radius = volley.Radius * round.SizeScale;
        const float sinceFired = age - (float)(round.BurstIndex * stats.Pattern.BurstInterval);

        if (sinceFired >= volley.Lifetime)
            continue;

        if (sinceFired < 0.0f)
        {
            QueueRound(now - sinceFired, round.Origin, velocity, volley.PlayerId,
                       volley.Lifetime, radius, 0, BASE_DAMAGE, stats.Homing);
            continue;
        }

        FireProjectile(Vector3Add(round.Origin, Vector3Scale(velocity, sinceFired)),
                       velocity, volley.PlayerId, volley.Lifetime - sinceFired, radius,
                       0, BASE_DAMAGE, stats.Homing);
    }
}

void World::QueueRound(double dueTime, Vector3 origin, Vector3 velocity, int ownerId,
                       float lifeTime, float radius, int pierce, float damage, bool homing)
{
    for (int i = 0; i < MAX_PENDING_ROUNDS; i++)
    {
        if (PendingRounds[i].active)
            continue;

        PendingRounds[i].active = true;
        PendingRounds[i].dueTime = dueTime;
        PendingRounds[i].origin = origin;
        PendingRounds[i].velocity = velocity;
        PendingRounds[i].ownerId = ownerId;
        PendingRounds[i].lifeTime = lifeTime;
        PendingRounds[i].radius = radius;
        PendingRounds[i].pierce = pierce;
        PendingRounds[i].damage = damage;
        PendingRounds[i].homing = homing;
        return;
    }
}

void World::UpdatePendingRounds(double now)
{
    for (int i = 0; i < MAX_PENDING_ROUNDS; i++)
    {
        if (!PendingRounds[i].active || now < PendingRounds[i].dueTime)
            continue;

        PendingRounds[i].active = false;
        ReleaseRound(PendingRounds[i].origin, PendingRounds[i].velocity, PendingRounds[i].ownerId,
                     PendingRounds[i].lifeTime, PendingRounds[i].radius,
                     PendingRounds[i].pierce, PendingRounds[i].damage, PendingRounds[i].homing);
    }
}

void World::DrawProjectiles()
{
    Camera3D camera = GameApp::GetInstance()->GetCamera();

    for (int i = 0; i < ProjectileHighWater; i++)
    {
        if (!Projectiles[i].active)
            continue;

        // Nothing this small is worth drawing from the far side of the world,
        // and the wide weapons put a lot of very small things in the air at once.
        if (Vector3DistanceSqr(camera.position, Projectiles[i].position) >
            PROJECTILE_DRAW_DISTANCE * PROJECTILE_DRAW_DISTANCE)
            continue;

        // Coarse on purpose. DrawSphere builds a sixteen-by-sixteen mesh a call,
        // which with a hundred rounds in the air was most of a frame.
        DrawSphereEx(Projectiles[i].position, Projectiles[i].radius * PROJECTILE_DRAW_RATIO,
                     4, 6, WHITE);
    }
}

// Walks the network's asteroid list once a frame, so each rotation is worked out
// once rather than once for every job that needs it.
void World::RefreshAsteroidFrame()
{
    NetClient& Net = GameApp::GetInstance()->GetNet();

    AsteroidFrameCount = 0;

    const int count = Net.GetAsteroidCount();
    for (int i = 0; i < count && AsteroidFrameCount < MAX_ASTEROIDS; i++)
    {
        Vector3 position = { 0.0f, 0.0f, 0.0f };
        Matrix rotation = MatrixIdentity();
        float scale = 1.0f;

        if (!Net.GetAsteroidSpatial(i, &position, &rotation, &scale))
            continue;

        AsteroidFrame& frame = AsteroidFrames[AsteroidFrameCount++];
        frame.Id = Net.GetAsteroidId(i);
        frame.Position = position;
        frame.Rotation = rotation;
        frame.Scale = scale;
        frame.Radius = Models::AsteroidRadiusLocal * scale;
        frame.BodyRadius = Models::AsteroidBodyRadius * scale;
    }
}

// Moves a world point into a model's own space, so the test gives the same answer
// whichever way the model is turned. Boxing it in the world is up to 75% wider.
static Vector3 ToLocalSpace(Vector3 worldPoint, Vector3 origin, const Matrix& rotation)
{
    return Vector3Transform(Vector3Subtract(worldPoint, origin), MatrixTranspose(rotation));
}

// How many points to test along one frame of travel, stepped by the round's own
// width. Testing only where it ended up would let a fast shot step over a ship.
static int SweepSamples(Vector3 from, Vector3 to, float radius)
{
    const int MAX_SAMPLES = 12;
    if (radius < 0.05f) radius = 0.05f;

    int samples = (int)(Vector3Distance(from, to) / radius) + 1;
    return samples > MAX_SAMPLES ? MAX_SAMPLES : samples;
}

void World::CheckProjectileCollisions()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

    const int localId = Net.GetLocalPlayerId();

    for (int p = 0; p < ProjectileHighWater; p++)
    {
        if (!Projectiles[p].active) continue;

        const float radius = Projectiles[p].radius;

        for (int a = 0; a < AsteroidFrameCount; a++)
        {
            AsteroidFrame& frame = AsteroidFrames[a];

            // Cheap reject against the whole step, widened so nothing close is
            // thrown away before the sweep gets a look at it.
            float reach = frame.Radius + radius
                        + Vector3Distance(Projectiles[p].previousPosition, Projectiles[p].position);
            if (Vector3DistanceSqr(frame.Position, Projectiles[p].position) > reach * reach)
                continue;

            // Exact: a sphere against the rock's own box, in the rock's space.
            BoundingBox body = Models::AsteroidBoxLocal;
            body.min = Vector3Scale(body.min, frame.Scale);
            body.max = Vector3Scale(body.max, frame.Scale);

            const int samples = SweepSamples(Projectiles[p].previousPosition, Projectiles[p].position, radius);
            bool hit = false;
            for (int step = 1; step <= samples && !hit; step++)
            {
                Vector3 along = Vector3Lerp(Projectiles[p].previousPosition, Projectiles[p].position,
                                            (float)step / (float)samples);
                hit = CheckCollisionBoxSphere(body, ToLocalSpace(along, frame.Position, frame.Rotation),
                                              radius);
            }

            if (!hit)
                continue;

            Sounds::PlayExplosion(frame.Position, PlayerShip.Position);

            // A piercing round carries on through and can break the next rock
            // too; anything else stops here.
            if (Projectiles[p].pierceLeft > 0)
                Projectiles[p].pierceLeft--;
            else
                Projectiles[p].active = false;

            // Only our own shots are reported, or the first client to notice
            // somebody else's would claim the rock. Their round still stops here.
            bool finished = false;
            if (Projectiles[p].ownerId == localId)
                finished = Net.ReportAsteroidHit(frame.Id, Projectiles[p].damage);

            // A rock leaves this frame's list only once finished, or the rest of
            // a volley would sail through what the first pellet just struck.
            if (finished)
                AsteroidFrames[a] = AsteroidFrames[--AsteroidFrameCount];
            break;
        }
    }
}

bool World::CheckShipCollisions()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

    for (int i = 0; i < AsteroidFrameCount; i++)
    {
        AsteroidFrame& frame = AsteroidFrames[i];

        float reach = frame.Radius + Models::ShipRadiusLocal;
        if (Vector3DistanceSqr(frame.Position, PlayerShip.Position) > reach * reach)
            continue;

        // The ship keeps its real shape, since the player steers it precisely;
        // the rock is near enough a ball to treat as one.
        Vector3 local = ToLocalSpace(frame.Position, PlayerShip.Position, PlayerShip.Rotation);
        if (!CheckCollisionBoxSphere(Models::ShipBoxLocal, local, frame.BodyRadius))
            continue;

        Sounds::PlayHurt(frame.Position, PlayerShip.Position);

        // The rock breaks either way, and the server works out what it costs us.
        // We do not respawn here: whether this was survivable is its to decide.
        Net.ReportAsteroidCollision(frame.Id);

        AsteroidFrames[i] = AsteroidFrames[--AsteroidFrameCount];
        return true;
    }

    return false;
}

// Our shots against everyone else's ships, judged by the shooter against the
// positions it is drawing, so a hit lands when it looks like it should.
bool World::CheckOutgoingFire()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

    const int localId = Net.GetLocalPlayerId();
    if (localId < 0)
        return false;

    for (int p = 0; p < ProjectileHighWater; p++)
    {
        if (!Projectiles[p].active) continue;

        // Only our own shots; everyone else scores their own.
        if (Projectiles[p].ownerId != localId) continue;

        const float radius = Projectiles[p].radius;
        const float travel = Vector3Distance(Projectiles[p].previousPosition, Projectiles[p].position);
        const int samples = SweepSamples(Projectiles[p].previousPosition, Projectiles[p].position, radius);

        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            Vector3 targetPos = { 0.0f, 0.0f, 0.0f };
            Matrix targetRot = MatrixIdentity();

            // Skips us and anyone not in the world yet.
            if (!Net.GetPlayerSpatial(i, &targetPos, &targetRot))
                continue;

            // Still drawn, but not shootable: their ship is sitting where we
            // last heard from them rather than where they actually are.
            if (Net.IsPlayerStale(i))
                continue;

            float reach = Models::ShipRadiusLocal + radius + travel;
            if (Vector3DistanceSqr(targetPos, Projectiles[p].position) > reach * reach)
                continue;

            bool hit = false;
            for (int step = 1; step <= samples && !hit; step++)
            {
                Vector3 along = Vector3Lerp(Projectiles[p].previousPosition, Projectiles[p].position,
                                            (float)step / (float)samples);
                hit = CheckCollisionBoxSphere(Models::ShipBoxLocal,
                                              ToLocalSpace(along, targetPos, targetRot),
                                              radius);
            }

            if (!hit)
                continue;

            Sounds::PlayExplosion(targetPos, PlayerShip.Position);

            if (Projectiles[p].pierceLeft > 0)
                Projectiles[p].pierceLeft--;
            else
                Projectiles[p].active = false;

            // The server takes the hull off them using our stored build, and ends
            // their run only if that was the last of it.
            Net.ReportHit(i);

            // One ship per round per frame. A piercing round gets the next on
            // the frame after, so one shot cannot empty a formation in a step.
            return true;
        }
    }

    return false;
}

void World::CheckCollisions()
{
    // Hitting a rock respawns us, which moves the ship, so stop there.
    if (CheckShipCollisions()) return;

    CheckOutgoingFire();
    CheckProjectileCollisions();
}

void World::DrawPlayerModels()
{
    NetClient& Net = GameApp::GetInstance()->GetNet();

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if (Net.GetPlayerSpatial(i, &pos, &rot))
        {
            Models::DrawModel(Models::ShipModel, pos, rot);
        }
    }
}

void World::DrawAsteroidModels()
{
    Camera3D camera = GameApp::GetInstance()->GetCamera();

    Vector3 viewForward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));

    // Asteroids the server has not put back yet can sit well outside the world,
    // and there is no point drawing those.
    const float cullDistance = ASTEROID_DRAW_DISTANCE;

    for (int i = 0; i < AsteroidFrameCount; i++)
    {
        const AsteroidFrame& frame = AsteroidFrames[i];

        Vector3 toAsteroid = Vector3Subtract(frame.Position, camera.position);
        float distanceSq = Vector3LengthSqr(toAsteroid);

        if (distanceSq > cullDistance * cullDistance)
            continue;

        // Behind the camera by more than its own radius, so it cannot be on
        // screen no matter how wide the field of view is.
        if (Vector3DotProduct(viewForward, toAsteroid) < -frame.Radius)
            continue;

        Models::DrawModel(Models::AsteroidModel, frame.Position, frame.Rotation, frame.Scale);
    }
}
