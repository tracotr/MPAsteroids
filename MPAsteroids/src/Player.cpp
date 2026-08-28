#include "include/Player.h"
#include "include/Models.h"
#include "include/Sounds.h"
#include "include/GameApp.h"
#include <iostream>

void Player::Update(double delta)
{   
    // Must match the thrust keys below, not the rotation ones.
    bool thrusting = IsKeyDown(KEY_W) || IsKeyDown(KEY_S);
    if (thrusting && !boosterActive)
    {
        boosterActive = true;
        Sounds::PlayBooster(Position, Position); 
    }
    else if (!thrusting && boosterActive)
    {
        boosterActive = false;
        Sounds::StopBooster();
    }

    laserCooldown += delta;
    laserCooldown = Clamp(laserCooldown, 0.0, LASER_COOLDOWN_DURATION);

    // Clicking with the pointer loose captures it, so the mouse only fires once
    // captured; otherwise the click that grabs the pointer also fires a shot.
    const bool mouseCaptured = GameApp::GetInstance()->IsMouseCaptured();
    bool firing = IsKeyDown(KEY_SPACE) || (mouseCaptured && IsMouseButtonDown(MOUSE_BUTTON_LEFT));

    isFiring = false;
    if (firing)
    {
        if(laserCooldown >= LASER_COOLDOWN_DURATION)
        {
            laserCooldown = 0;
            Sounds::PlayLaser(Position, Position);
            isFiring = true;
        }
    }
    
    float currentRotationSpeed = RotationSpeed;

    // Our ship rotates slower when holding down the shift key
    if (IsKeyDown(KEY_LEFT_SHIFT))
    {
        currentRotationSpeed = SlowRotationSpeed;
    }

    // Mouse look, only while the pointer is captured. GetMouseDelta is already
    // movement since the last frame, so this needs no scaling by delta the way a
    // held key would. Signs follow the keys below: right yaws like D, and pushing
    // the mouse away pitches like R.
    if (mouseCaptured)
    {
        Vector2 look = GetMouseDelta();
        float sensitivity = MOUSE_SENSITIVITY;
        if (IsKeyDown(KEY_LEFT_SHIFT)) sensitivity *= MOUSE_FINE_FACTOR;

        if (look.x != 0.0f) Rotation = MatrixMultiply(MatrixRotateY(-look.x * sensitivity), Rotation);
        if (look.y != 0.0f) Rotation = MatrixMultiply(MatrixRotateX(-look.y * sensitivity), Rotation);
    }

    if (IsKeyDown(KEY_R)) Rotation = MatrixMultiply(MatrixRotateX(currentRotationSpeed), Rotation);
    if (IsKeyDown(KEY_F)) Rotation = MatrixMultiply(MatrixRotateX(-currentRotationSpeed), Rotation);

    if (IsKeyDown(KEY_A)) Rotation = MatrixMultiply(MatrixRotateY(currentRotationSpeed), Rotation);
    if (IsKeyDown(KEY_D)) Rotation = MatrixMultiply(MatrixRotateY(-currentRotationSpeed), Rotation);

    if (IsKeyDown(KEY_Q)) Rotation = MatrixMultiply(MatrixRotateZ(currentRotationSpeed), Rotation);
    if (IsKeyDown(KEY_E)) Rotation = MatrixMultiply(MatrixRotateZ(-currentRotationSpeed), Rotation);

    if (IsKeyDown(KEY_W) && Vector3LengthSqr(Velocity) <= MaxSpeed)
    {
        Vector3 forward = Vector3Transform(Forward, Rotation);
        Velocity = Vector3Add(Velocity, forward);
    }

    if (IsKeyDown(KEY_S) && Vector3LengthSqr(Velocity) <= MaxSpeed / 2)
    {
        Vector3 forward = Vector3Transform(Backward, Rotation);
        Velocity = Vector3Add(Velocity, forward);
    }

	Vector3 friction = Vector3Scale(Vector3Normalize(Velocity), -2.5f * (float)delta);

	if (Vector3LengthSqr(friction) < Vector3LengthSqr(Velocity))
        Velocity = Vector3Add(Velocity, friction);
	else	
        Velocity = { 0.0f, 0.0f, 0.0f };

    Position = Vector3Add(Position, Vector3Scale(Velocity, (float)delta));

    if (IsKeyDown(KEY_Y)) Reset();
}

void Player::Draw()
{
    Models::DrawModel(Models::ShipModel, this->Position, this->Rotation);
}

void Player::Reset()
{
    Alive = true;
    Respawn();
}

namespace
{
    // Tries a few random spots and takes the best one, rather than using a fixed
    // set of spawn points, because the things to avoid keep moving.
    const int SPAWN_CANDIDATES = 24;

    // A spot this clear is good enough; no need to check the rest. Roughly half
    // a second of laser flight, so a fresh ship has a moment before anyone
    // already nearby can reach it.
    const float SPAWN_GOOD_ENOUGH = 14.0f;

    float RandomUnitFloat()
    {
        return (float)GetRandomValue(-1000, 1000) / 1000.0f;
    }

    Vector3 RandomSpawnCandidate()
    {
        Vector3 direction = { 0.0f, 0.0f, 1.0f };

        for (int attempt = 0; attempt < 12; attempt++)
        {
            Vector3 candidate = { RandomUnitFloat(), RandomUnitFloat(), RandomUnitFloat() };
            float lengthSq = Vector3LengthSqr(candidate);
            if (lengthSq > 0.0001f && lengthSq <= 1.0f)
            {
                direction = Vector3Scale(candidate, 1.0f / sqrtf(lengthSq));
                break;
            }
        }

        float radius = PLAYER_SPAWN_RADIUS * (0.7f + 0.3f * (float)GetRandomValue(0, 1000) / 1000.0f);
        return Vector3Scale(direction, radius);
    }

    // How much space a spot has: the distance to the nearest player or asteroid.
    float SpawnClearance(Vector3 candidate)
    {
        NetClient& Net = GameApp::GetInstance()->GetNet();

        float clearance = PLAYER_SPAWN_RADIUS * 4.0f;

        for (int i = 0; i < MAX_PLAYERS; i++)
        {
            Vector3 position = { 0.0f, 0.0f, 0.0f };
            Matrix rotation = MatrixIdentity();

            // Skips the local player and anyone not in the world yet.
            if (!Net.GetPlayerSpatial(i, &position, &rotation))
                continue;

            clearance = fminf(clearance, Vector3Distance(position, candidate));
        }

        const int asteroidCount = Net.GetAsteroidCount();
        for (int i = 0; i < asteroidCount; i++)
        {
            Vector3 position = { 0.0f, 0.0f, 0.0f };
            Matrix rotation = MatrixIdentity();
            float scale = 1.0f;

            if (!Net.GetAsteroidSpatial(i, &position, &rotation, &scale))
                continue;

            // Measured to the rock's surface, so bigger ones are given more room.
            clearance = fminf(clearance, Vector3Distance(position, candidate) - Models::AsteroidRadiusLocal * scale);
        }

        return clearance;
    }

    // Turns the ship back toward the middle of the play area, so spawning out on
    // the edge does not leave the player staring into empty space. The model's
    // local forward is -Z, which is why the Z axis gets the opposite of the
    // direction we want to point.
    Matrix LookTowardOrigin(Vector3 from)
    {
        Vector3 forward = Vector3Subtract((Vector3){ 0.0f, 0.0f, 0.0f }, from);
        if (Vector3LengthSqr(forward) < 0.0001f)
            return MatrixIdentity();

        forward = Vector3Normalize(forward);

        Vector3 zAxis = Vector3Negate(forward);
        Vector3 upHint = (fabsf(zAxis.y) > 0.99f) ? (Vector3){ 1.0f, 0.0f, 0.0f } : (Vector3){ 0.0f, 1.0f, 0.0f };

        Vector3 xAxis = Vector3Normalize(Vector3CrossProduct(upHint, zAxis));
        Vector3 yAxis = Vector3CrossProduct(zAxis, xAxis);

        Matrix result = MatrixIdentity();
        result.m0 = xAxis.x; result.m1 = xAxis.y; result.m2 = xAxis.z;
        result.m4 = yAxis.x; result.m5 = yAxis.y; result.m6 = yAxis.z;
        result.m8 = zAxis.x; result.m9 = zAxis.y; result.m10 = zAxis.z;
        return result;
    }
}

void Player::Respawn()
{
    // Picks the emptiest of a few spots out from the centre, so ships neither
    // stack on each other nor reappear inside an asteroid.
    Vector3 best = RandomSpawnCandidate();
    float bestClearance = SpawnClearance(best);

    for (int attempt = 1; attempt < SPAWN_CANDIDATES && bestClearance < SPAWN_GOOD_ENOUGH; attempt++)
    {
        Vector3 candidate = RandomSpawnCandidate();
        float clearance = SpawnClearance(candidate);

        if (clearance > bestClearance)
        {
            best = candidate;
            bestClearance = clearance;
        }
    }

    Position = best;
    Rotation = LookTowardOrigin(best);
    Velocity = Vector3{ 0.0f, 0.0f, 0.0f };
    boosterActive = false;
    Sounds::StopBooster();
}

Vector3 Player::GetForwardVector()
{
    return Vector3Normalize(Vector3Transform((Vector3){ 0, 0, -1 }, this->Rotation));
}