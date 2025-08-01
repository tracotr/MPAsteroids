#include "include/World.h"

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
    NetClient.NetConnect("127.0.0.1");
    PlayerShip.Reset();
}

void World::Update(double delta)
{
    PlayerShip.Update(delta);
    
    NetClient.UpdateLocalPlayer(PlayerShip.Position, PlayerShip.Rotation);
    NetClient.NetUpdate(GetTime(), delta);
    CreateAsteroidCollision();
    CheckCollisions();
}

void World::Draw()
{
    Models::DrawSkybox();

    PlayerShip.Draw();
    DrawPlayerModels();
    DrawAsteroidModels();
    DrawShipLaser();
}

void World::DrawUI(Camera camera)
{
    Models::DrawUI(camera, PlayerShip.Velocity, PlayerShip.Position, NetClient.GetLocalPlayerId(), NetClient.GetScoreboard());
}

void World::DrawPlayerModels()
{
    // draw other player models
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(NetClient.GetPlayerSpatial(i, &pos, &rot))
        {   
            Models::DrawModel(Models::ShipModel, pos, rot);
        }
    }
}

void World::DrawAsteroidModels()
{
    // Draw asteroid models
    for(int i = 0; i < NetClient.GetMaxAsteroids(); i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(NetClient.GetAsteroidSpatial(i, &pos, &rot))
        {   
            Models::DrawModel(Models::AsteroidModel, pos, rot);
        }
    }
}

void World::DrawShipLaser()
{
    Vector3 start = PlayerShip.Position;
    Vector3 direction = PlayerShip.GetForwardVector();

    double distance = 50.0f;

    for(int i = 0; i < NetClient.GetMaxAsteroids(); i++)
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
    for(int i = 0; i < NetClient.GetMaxAsteroids(); i++)
    {
        // calculating bounding boxes
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(NetClient.GetAsteroidSpatial(i, &pos, &rot))
        {   
            AsteroidBoundingBoxes[i] = Models::GetWorldBoundingBox(Models::AsteroidBoxLocal, pos, rot);
        }
    }
}

void World::CheckShipCollisions(BoundingBox asteroidBox)
{
    BoundingBox PlayerBox = Models::GetWorldBoundingBox(Models::ShipBoxLocal, PlayerShip.Position, PlayerShip.Rotation);

    // check player-asteroid collisions
    if(CheckCollisionBoxes(PlayerBox, asteroidBox))
    {
        PlayerShip.Respawn();
        NetClient.HandlePlayerCollision();
    }
}

void World::CheckLaserCollisions(BoundingBox asteroidBox, int asteroidId)
{
    if(!PlayerShip.isFiring){
        return;
    }

    Vector3 start = PlayerShip.Position;
    Vector3 direction = PlayerShip.GetForwardVector();
    Ray laser = { start, direction };
    // Calculate bounding boxes for asteroids, and check collisions

    // check player-asteroid collisions
    if(GetRayCollisionBox(laser, asteroidBox).hit)
    {
        NetClient.HandleDestroyAsteroid(NetClient.GetLocalPlayerId(), asteroidId);
        PlayerShip.isFiring = false;
    }
}

void World::CheckCollisions()
{
    for(int i = 0; i < NetClient.GetMaxAsteroids(); i++)
    {
        CheckShipCollisions(AsteroidBoundingBoxes[i]);
        CheckLaserCollisions(AsteroidBoundingBoxes[i], i);
    }
}

