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
    // Networking is started separately (see main.cpp's connect menu) so the
    // player can choose who to connect to instead of always hitting localhost.
    PlayerShip.Reset();
}

void World::Update(double delta)
{
    PlayerShip.Update(delta);
    
    Net.UpdateLocalPlayer(PlayerShip.Position, PlayerShip.Rotation);
    Net.NetUpdate(GetTime(), delta);
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
    Models::DrawUI(camera, PlayerShip.Velocity, PlayerShip.Position, Net.GetLocalPlayerId(), Net.GetScoreboard(), Net.GetPlayerNames());
}

void World::DrawPlayerModels()
{
    // draw other player models
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(Net.GetPlayerSpatial(i, &pos, &rot))
        {   
            Models::DrawModel(Models::ShipModel, pos, rot);
        }
    }
}

void World::DrawAsteroidModels()
{
    // Draw asteroid models
    for(int i = 0; i < Net.GetMaxAsteroids(); i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        float scale = 1.0f;
        if(Net.GetAsteroidSpatial(i, &pos, &rot, &scale))
        {   
            Models::DrawModel(Models::AsteroidModel, pos, rot, scale);
        }
    }
}

void World::DrawShipLaser()
{
    Vector3 start = PlayerShip.Position;
    Vector3 direction = PlayerShip.GetForwardVector();

    double distance = 50.0f;

    for(int i = 0; i < Net.GetMaxAsteroids(); i++)
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
    for(int i = 0; i < Net.GetMaxAsteroids(); i++)
    {
        // calculating bounding boxes
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        float scale = 1.0f;
        if(Net.GetAsteroidSpatial(i, &pos, &rot, &scale))
        {   
            BoundingBox localBox = Models::AsteroidBoxLocal;
            localBox.min = Vector3Scale(localBox.min, scale);
            localBox.max = Vector3Scale(localBox.max, scale);
            AsteroidBoundingBoxes[i] = Models::GetWorldBoundingBox(localBox, pos, rot);
        }
    }
}

void World::CheckShipCollisions(BoundingBox asteroidBox)
{
    BoundingBox PlayerBox = Models::GetWorldBoundingBox(Models::ShipBoxLocal, PlayerShip.Position, PlayerShip.Rotation);

    // check player-asteroid collisions
    if(CheckCollisionBoxes(PlayerBox, asteroidBox))
    {
        Vector3 hitPosition = PlayerShip.Position;
        PlayerShip.Respawn();
        Sounds::PlayHurt(hitPosition, PlayerShip.Position);
        Net.HandlePlayerCollision();
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
        Vector3 asteroidPosition = { 0.0f, 0.0f, 0.0f };
        Matrix asteroidRotation = MatrixIdentity();
        if (Net.GetAsteroidSpatial(asteroidId, &asteroidPosition, &asteroidRotation))
        {
            Sounds::PlayExplosion(asteroidPosition, PlayerShip.Position);
        }

        Net.HandleDestroyAsteroid(Net.GetLocalPlayerId(), asteroidId);
        PlayerShip.isFiring = false;
    }
}

void World::CheckCollisions()
{
    for(int i = 0; i < Net.GetMaxAsteroids(); i++)
    {
        CheckShipCollisions(AsteroidBoundingBoxes[i]);
        CheckLaserCollisions(AsteroidBoundingBoxes[i], i);
    }
}

