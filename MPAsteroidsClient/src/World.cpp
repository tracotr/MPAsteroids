#include "include/World.h"
#include "include/Models.h"

#include "include/NetClient.h"
#include "include/networking/NetConstants.h"

#include <stdio.h>

World* World::Instance = nullptr;

World& World::Create()
{
    if (!Instance)
        Instance = new World();

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
    PlayerShip.Reset();
}

void World::Update(double delta)
{
    PlayerShip.Update(delta);
    UpdateLocalPlayer(PlayerShip.Position, PlayerShip.Rotation);

    CreateAsteroidCollision();
    CheckShipCollisions();
    CheckLaserCollisions();
}

void World::DrawModels()
{
    Models::DrawSkybox();

    PlayerShip.Draw();
    DrawPlayerModels();
    DrawAsteroidModels();
    DrawShipLaser();
}

void World::DrawPlayerModels()
{
    // draw other player models
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(GetPlayerSpatial(i, &pos, &rot))
        {   
            Models::Draw(Models::ShipModel, pos, rot);
        }
    }
}

void World::DrawAsteroidModels()
{
    // Draw asteroid models
    for(int i = 0; i < GetMaxAsteroids(); i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(GetAsteroidSpatial(i, &pos, &rot))
        {   
            Models::Draw(Models::AsteroidModel, pos, rot);
        }
    }
}

void World::DrawShipLaser()
{
    Vector3 start = PlayerShip.Position;
    Vector3 direction = PlayerShip.GetForwardVector();

    double distance = 50.0f;

    for(int i = 0; i < GetMaxAsteroids(); i++)
    {
        Ray ray = { start, direction };
        RayCollision hit = GetRayCollisionBox(ray, AsteroidBoundingBoxes[i]);
        if (hit.hit && hit.distance < distance) {
            distance = hit.distance;
        }
    }

    Vector3 end = Vector3Add(start, Vector3Scale(direction, distance));

    DrawLine3D(start, end, RAYWHITE);
}

void World::CreateAsteroidCollision()
{
    for(int i = 0; i < GetMaxAsteroids(); i++)
    {
        // calculating bounding boxes
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(GetAsteroidSpatial(i, &pos, &rot))
        {   
            AsteroidBoundingBoxes[i] = Models::GetWorldBoundingBox(Models::AsteroidBoxLocal, pos, rot);
        }
    }
}

void World::CheckShipCollisions()
{
    BoundingBox PlayerBox = Models::GetWorldBoundingBox(Models::ShipBoxLocal, PlayerShip.Position, PlayerShip.Rotation);
    // Calculate bounding boxes for asteroids, and check collisions
    for(int i = 0; i < GetMaxAsteroids(); i++)
    {
        // check player-asteroid collisions
        if(CheckCollisionBoxes(PlayerBox, AsteroidBoundingBoxes[i]))
        {
            PlayerShip.Respawn();
            printf("Player collision asteroid %i\n", i);
            break;
        }
    }
}

void World::CheckLaserCollisions()
{
    if(!PlayerShip.isFiring){
        return;
    }

    Vector3 start = PlayerShip.Position;
    Vector3 direction = PlayerShip.GetForwardVector();
    Ray laser = { start, direction };
    // Calculate bounding boxes for asteroids, and check collisions
    for(int i = 0; i < GetMaxAsteroids(); i++)
    {
        // check player-asteroid collisions
        if(GetRayCollisionBox(laser, AsteroidBoundingBoxes[i]).hit)
        {
            HandleDestroyAsteroid(GetLocalPlayerId(), i);
            PlayerShip.isFiring = false;
            break;
        }
    }
}

void World::DrawUI(Camera camera)
{
    DrawText(TextFormat("Velocity: %03.03f", Vector3LengthSqr(PlayerShip.Velocity)), 20, 20, 20, RAYWHITE);
    DrawText(TextFormat("Position: %03.03f, %03.03f, %03.03f", PlayerShip.Position.x, PlayerShip.Position.y, PlayerShip.Position.z), 20, 40, 20, RAYWHITE);
    DrawText(TextFormat("ID: %i", GetLocalPlayerId()), 20, 60, 20, RAYWHITE);
}
