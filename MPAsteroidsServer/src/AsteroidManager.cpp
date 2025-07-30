#include "include/AsteroidManager.h"
#include "include/NetworkUtil.h"

//  updates position of asteroid and sends to players
void AsteroidManager::UpdateAsteroids(PlayerInfo (&players)[MAX_PLAYERS], double delta)
{
    // Update position of asteroids
    for(int i = 0; i < AsteroidAmount; i++)
    {
        AsteroidInfo& asteroid = Asteroids[i];
        asteroid.Position = Vector3Add(asteroid.Position, Vector3Scale(asteroid.Velocity, delta));
        Vector3 closestPlayerPos = GetNearestPlayerPosition(players, asteroid.Position);
        WrapAsteroid(&asteroid, closestPlayerPos, MAX_ASTEROID_DIST); 
    }

    // send position to players
    AsteroidInfoPacket buffer = { 0 };
    buffer.Command = UpdateAsteroid;
    memcpy(buffer.AllAsteroids, Asteroids, sizeof(Asteroids));
    buffer.AsteroidCount = AsteroidAmount;

    // create packet
    ENetPacket* packet = enet_packet_create(&buffer, sizeof(buffer), ENET_PACKET_FLAG_UNSEQUENCED);
    // send to all connected clients
    NetworkUtil::SendPacketToAllBut(packet, -1, 1);
}

// returns true if asteroid exists, gives pos and rot to a pointer
bool AsteroidManager::GetAsteroidSpatial(int id, Vector3* pos, Matrix* rot)
{
    if(id < 0 || id >= AsteroidAmount)
    {
        return false;
    }

    *pos = Asteroids[id].Position;
    *rot = Asteroids[id].Rotation;
    return true;
}

// creates a new asteroid with random position
AsteroidInfo AsteroidManager::CreateAsteroid()
{
    AsteroidInfo newAsteroid = { 0 };
    newAsteroid.Position = (Vector3){ RandBetween(-25, 25), RandBetween(-25, 25), RandBetween(-25, 25) };
    newAsteroid.Velocity = (Vector3){ RandBetween(-2, 2), RandBetween(-2, 2), RandBetween(-2, 2) };
    newAsteroid.Rotation = MatrixIdentity();
    return newAsteroid;
}

// respawn an asteroid with an id 
void AsteroidManager::RespawnAsteroid(int id)
{
    Asteroids[id] = CreateAsteroid(); 
}

// gives a rand val between min and max
float AsteroidManager::RandBetween(float min, float max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}

// spawns x amount of asteroids
void AsteroidManager::SpawnAsteroids(int amount)
{
    // spawn amount of asteroids 
    for(int i = 0; i < amount; i++)
    {   
        // skip if we're over max
        if(AsteroidAmount >= MAX_ASTEROIDS){
            break;
        }

        AsteroidInfo newAsteroid = CreateAsteroid();
        Asteroids[AsteroidAmount] = newAsteroid;
        AsteroidAmount += 1;
    }
}

// finds the nearest player and returns position (used for wrapping)
Vector3 AsteroidManager::GetNearestPlayerPosition(PlayerInfo (&players)[MAX_PLAYERS], Vector3 asteroidPos)
{
    float closestDistSq = MAX_SQR_V3;
    Vector3 closestPos = { 0 };

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (!players[i].Active)
            continue;

        float distSq = Vector3DistanceSqr(players[i].Position, asteroidPos);
        if (distSq < closestDistSq)
        {
            closestDistSq = distSq;
            closestPos = players[i].Position;
        }
    }

    return closestPos;
}

// wraps the asteroid to the opposite side of a vector, like a giant square around it
void AsteroidManager::WrapAsteroid(AsteroidInfo* asteroid, Vector3 center, float wrapDistance)
{
    Vector3& pos = asteroid->Position;

    if (pos.x > center.x + wrapDistance)
        pos.x = center.x - wrapDistance;
    else if (pos.x < center.x - wrapDistance)
        pos.x = center.x + wrapDistance;

    if (pos.y > center.y + wrapDistance)
        pos.y = center.y - wrapDistance;
    else if (pos.y < center.y - wrapDistance)
        pos.y = center.y + wrapDistance;

    if (pos.z > center.z + wrapDistance)
        pos.z = center.z - wrapDistance;
    else if (pos.z < center.z - wrapDistance)
        pos.z = center.z + wrapDistance;
}
