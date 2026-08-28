#include "include/World.h"
#include "include/GameApp.h"

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
        Vector3 velocity = Vector3Scale(PlayerShip.GetForwardVector(), PlayerShip.LASER_SPEED);
        FireProjectile(PlayerShip.Position, velocity, Net.GetLocalPlayerId());
        Net.SendProjectile(PlayerShip.Position, velocity);
    }

    // Shots that arrived while this tab was in the background have been sitting
    // in the queue. Spawning them as-is puts a whole volley back at its firing
    // point at once, so each is caught up to where it should be by now, and any
    // that would already have faded is dropped rather than replayed.
    for (int i = 0; i < Net.RemoteProjectileCount; i++) {
        const NetClient::RemoteProjectileEvent& shot = Net.RemoteProjectilesQueue[i];
        const float age = (float)Net.QueuedProjectileAge(i);

        if (age >= PROJECTILE_LIFETIME)
            continue;

        Vector3 caughtUp = Vector3Add(shot.Position, Vector3Scale(shot.Velocity, age));
        FireProjectile(caughtUp, shot.Velocity, shot.PlayerId, PROJECTILE_LIFETIME - age);
    }
    Net.RemoteProjectileCount = 0;
    
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
    Camera3D camera = GameApp::GetInstance()->GetCamera();
    
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

    Models::DrawUI(camera, PlayerShip.Velocity, PlayerShip.Position, Net.GetLocalPlayerId(), Net.GetScoreboard(), Net.GetPlayerNames());

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
            otherPlayerCount++;
        }
    }

    DrawPlayerIndicators(otherPlayersData, otherPlayerCount, GetScreenWidth(), GetScreenHeight());
}


void World::DrawPlayerIndicators(const PlayerIndicator* otherPlayersData, int otherPlayerCount, int screenWidth, int screenHeight) {
    Camera3D camera = GameApp::GetInstance()->GetCamera();

    Vector2 screenCenter = { (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
    float edgeMargin = 40.0f;

    for (int i = 0; i < otherPlayerCount; i++) {
        Vector3 targetPos = otherPlayersData[i].position;
        const char* playerName = otherPlayersData[i].name;

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
            
            DrawText(playerName, (int)(clampedPos.x - 15), (int)(clampedPos.y + 12), 10, RAYWHITE);
            DrawText(TextFormat("%0.0fm", distance), (int)(clampedPos.x - 15), (int)(clampedPos.y + 22), 10, LIGHTGRAY);
        } else {            
            DrawText(playerName, (int)(screenPos.x - 15), (int)(screenPos.y - 38), 10, RAYWHITE);
            DrawText(TextFormat("%0.0fm", distance), (int)(screenPos.x - 15), (int)(screenPos.y - 28), 10, LIGHTGRAY);
        }
    }
}

void World::UpdateProjectiles(double delta)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (Projectiles[i].active)
        {
            Projectiles[i].previousPosition = Projectiles[i].position;
            Projectiles[i].position = Vector3Add(Projectiles[i].position, Vector3Scale(Projectiles[i].velocity, delta));
            Projectiles[i].lifeTime -= delta;
            
            if (Projectiles[i].lifeTime <= 0.0f)
            {
                Projectiles[i].active = false;
            }
        }
    }
}

void World::FireProjectile(Vector3 position, Vector3 velocity, int ownerId, float lifeTime)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (!Projectiles[i].active)
        {
            Projectiles[i].active = true;
            Projectiles[i].ownerId = ownerId;
            Projectiles[i].position = position;
            Projectiles[i].previousPosition = position;
            Projectiles[i].velocity = velocity;
            Projectiles[i].lifeTime = lifeTime;
            break;
        }
    }
}

void World::DrawProjectiles()
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (Projectiles[i].active)
        {
            DrawSphere(Projectiles[i].position, 0.1f, WHITE);
        }
    }
}

// Walks the network's asteroid list once a frame. Drawing and both collision
// checks read from this instead of asking again, so each asteroid's rotation is
// worked out once a frame rather than once for every job that needs it.
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

// Moves a world point into a model's own space. These matrices only rotate, so
// transposing one is the same as inverting it. Testing in that space gives the
// same answer whichever way the model is turned. Boxing a rotated model in the
// world instead makes the box up to 75% wider, so a spinning asteroid would
// reach out and grab players from most of a rock's width away.
static Vector3 ToLocalSpace(Vector3 worldPoint, Vector3 origin, const Matrix& rotation)
{
    return Vector3Transform(Vector3Subtract(worldPoint, origin), MatrixTranspose(rotation));
}

// How many points to test along one frame of travel. A shot covers half a unit
// per frame at 60fps but three units at the frame-time cap, which is wider than
// a ship, so testing only where it ended up would let it step clean over.
static int SweepSamples(Vector3 from, Vector3 to)
{
    const int MAX_SAMPLES = 8;
    int samples = (int)(Vector3Distance(from, to) / PROJECTILE_HIT_RADIUS) + 1;
    return samples > MAX_SAMPLES ? MAX_SAMPLES : samples;
}

void World::CheckProjectileCollisions()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

    for (int p = 0; p < MAX_PROJECTILES; p++)
    {
        if (!Projectiles[p].active) continue;

        for (int a = 0; a < AsteroidFrameCount; a++)
        {
            AsteroidFrame& frame = AsteroidFrames[a];

            // Cheap reject against the whole step, widened so nothing close is
            // thrown away before the sweep gets a look at it.
            float reach = frame.Radius + PROJECTILE_HIT_RADIUS
                        + Vector3Distance(Projectiles[p].previousPosition, Projectiles[p].position);
            if (Vector3DistanceSqr(frame.Position, Projectiles[p].position) > reach * reach)
                continue;

            // Exact: a sphere against the rock's own box, in the rock's space.
            BoundingBox body = Models::AsteroidBoxLocal;
            body.min = Vector3Scale(body.min, frame.Scale);
            body.max = Vector3Scale(body.max, frame.Scale);

            const int samples = SweepSamples(Projectiles[p].previousPosition, Projectiles[p].position);
            bool hit = false;
            for (int step = 1; step <= samples && !hit; step++)
            {
                Vector3 along = Vector3Lerp(Projectiles[p].previousPosition, Projectiles[p].position,
                                            (float)step / (float)samples);
                hit = CheckCollisionBoxSphere(body, ToLocalSpace(along, frame.Position, frame.Rotation),
                                              PROJECTILE_HIT_RADIUS);
            }

            if (!hit)
                continue;

            Projectiles[p].active = false;
            Sounds::PlayExplosion(frame.Position, PlayerShip.Position);

            Net.ReportAsteroidDestroyed(frame.Id);

            // Dropped from this frame's list so nothing else can hit the same
            // asteroid before the server's next broadcast arrives.
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

        Vector3 hitPosition = PlayerShip.Position;
        PlayerShip.Respawn();
        Sounds::PlayHurt(hitPosition, PlayerShip.Position);

        Net.HandlePlayerCollision();
        Net.ReportAsteroidDestroyed(frame.Id);

        AsteroidFrames[i] = AsteroidFrames[--AsteroidFrameCount];
        return true;
    }

    return false;
}

// Our shots against everyone else's ships. The shooter judges this, not the
// target: it tests against the very positions it is drawing, so a hit lands when
// it looks like it should. Letting the target decide meant nothing registered
// while its browser tab was in the background and had stopped running frames.
bool World::CheckOutgoingFire()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

    const int localId = Net.GetLocalPlayerId();
    if (localId < 0)
        return false;

    for (int p = 0; p < MAX_PROJECTILES; p++)
    {
        if (!Projectiles[p].active) continue;

        // Only our own shots; everyone else scores their own.
        if (Projectiles[p].ownerId != localId) continue;

        const float travel = Vector3Distance(Projectiles[p].previousPosition, Projectiles[p].position);
        const int samples = SweepSamples(Projectiles[p].previousPosition, Projectiles[p].position);

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

            float reach = Models::ShipRadiusLocal + PROJECTILE_HIT_RADIUS + travel;
            if (Vector3DistanceSqr(targetPos, Projectiles[p].position) > reach * reach)
                continue;

            bool hit = false;
            for (int step = 1; step <= samples && !hit; step++)
            {
                Vector3 along = Vector3Lerp(Projectiles[p].previousPosition, Projectiles[p].position,
                                            (float)step / (float)samples);
                hit = CheckCollisionBoxSphere(Models::ShipBoxLocal,
                                              ToLocalSpace(along, targetPos, targetRot),
                                              PROJECTILE_HIT_RADIUS);
            }

            if (!hit)
                continue;

            Projectiles[p].active = false;
            Sounds::PlayExplosion(targetPos, PlayerShip.Position);

            // The server clears their score, credits ours, and tells them to respawn.
            Net.ReportKill(i);
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
