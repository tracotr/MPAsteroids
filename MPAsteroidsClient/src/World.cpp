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

void World::Update()
{
    PlayerShip.Update();
    UpdateLocalPlayer(PlayerShip.Position, PlayerShip.Rotation);
}

void World::DrawModels()
{
    Models::DrawSkybox();

    PlayerShip.Draw();

    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(GetPlayerSpatial(i, &pos, &rot))
        {   
            Models::Draw(Models::ShipModel, pos, rot);
        }
    }

    for (int i = 0; i < GetMaxAsteroids(); i++)
    {
        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        if(GetAsteroidSpatial(i, &pos, &rot))
        {   
            Models::Draw(Models::AsteroidModel, pos, rot);
        }
    }
}

void World::DrawUI(Camera camera)
{
    return;
}