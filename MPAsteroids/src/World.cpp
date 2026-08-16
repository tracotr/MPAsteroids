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
        FireProjectile(PlayerShip.Position, velocity);
        Net.SendProjectile(PlayerShip.Position, velocity);
    }

    for (int i = 0; i < Net.RemoteProjectileCount; i++) {
        FireProjectile(Net.RemoteProjectilesQueue[i].Position, Net.RemoteProjectilesQueue[i].Velocity);
    }
    Net.RemoteProjectileCount = 0;
    
    UpdateProjectiles(delta);
    
    Net.UpdateLocalPlayer(PlayerShip.Position, PlayerShip.Rotation);
    Net.NetUpdate(GetTime(), delta);
    CreateAsteroidCollision();
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
}

void World::UpdateProjectiles(double delta)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (Projectiles[i].active)
        {
            Projectiles[i].position = Vector3Add(Projectiles[i].position, Vector3Scale(Projectiles[i].velocity, delta));
            Projectiles[i].lifeTime -= delta;
            
            if (Projectiles[i].lifeTime <= 0.0f)
            {
                Projectiles[i].active = false;
            }
        }
    }
}

void World::FireProjectile(Vector3 position, Vector3 velocity)
{
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        if (!Projectiles[i].active)
        {
            Projectiles[i].active = true;
            Projectiles[i].position = position;
            Projectiles[i].velocity = velocity;
            Projectiles[i].lifeTime = 2.0f;
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
            DrawSphere(Projectiles[i].position, 0.15f, WHITE);
        }
    }
}

void World::CheckProjectileCollisions()
{
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();
    
    for (int p = 0; p < MAX_PROJECTILES; p++)
    {
        if (!Projectiles[p].active) continue;

        for (int a = 0; a < Net.GetMaxAsteroids(); a++)
        {
            if (CheckCollisionBoxSphere(AsteroidBoundingBoxes[a], Projectiles[p].position, 0.5f))
            {
                Projectiles[p].active = false; 

                Vector3 asteroidPosition = { 0.0f, 0.0f, 0.0f };
                Matrix asteroidRotation = MatrixIdentity();
                if (Net.GetAsteroidSpatial(a, &asteroidPosition, &asteroidRotation))
                {
                    Sounds::PlayExplosion(asteroidPosition, PlayerShip.Position);
                }

                Net.HandleDestroyAsteroid(Net.GetLocalPlayerId(), a);
                
                AsteroidBoundingBoxes[a].min = { 0.0f, 0.0f, 0.0f };
                AsteroidBoundingBoxes[a].max = { 0.0f, 0.0f, 0.0f };
                
                break; 
            }
        }
    }
}
void World::DrawPlayerModels()
{   
    NetClient& Net = GameApp::GetInstance()->GetNet();

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
    NetClient& Net = GameApp::GetInstance()->GetNet();

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

void World::CreateAsteroidCollision()
{
    NetClient& Net = GameApp::GetInstance()->GetNet();

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
    Player& PlayerShip = GameApp::GetInstance()->GetPlayer();
    NetClient& Net = GameApp::GetInstance()->GetNet();

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

void World::CheckCollisions()
{
    NetClient& Net = GameApp::GetInstance()->GetNet();

    for(int i = 0; i < Net.GetMaxAsteroids(); i++)
    {
        CheckShipCollisions(AsteroidBoundingBoxes[i]);
    }
    
    CheckProjectileCollisions();
}

