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
    PlayerShip.Reset();
}

void World::Update(double delta, Camera3D camera)
{
    PlayerShip.Update(delta, camera);
    
    Net.UpdateLocalPlayer(PlayerShip.Position, PlayerShip.Rotation);
    Net.NetUpdate(GetTime(), delta);
    CreateAsteroidCollision();
    CheckCollisions(camera);
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

    auto playerNames = Net.GetPlayerNames();
    std::vector<std::pair<Vector3, std::string>> otherPlayersData;
    for (int i = 0; i < MAX_PLAYERS; i++)
    {
        if (i == Net.GetLocalPlayerId()) continue;

        Vector3 pos = { 0.0f, 0.0f, 0.0f };
        Matrix rot = MatrixIdentity();
        
        if (Net.GetPlayerSpatial(i, &pos, &rot))
        {
            otherPlayersData.push_back({pos, playerNames[i]});
        }
    }

    DrawPlayerIndicators(camera, otherPlayersData, GetScreenWidth(), GetScreenHeight());
}


void World::DrawPlayerIndicators(Camera3D camera, const std::vector<std::pair<Vector3, std::string>>& otherPlayersData, int screenWidth, int screenHeight) {
    Vector2 screenCenter = { (float)screenWidth / 2.0f, (float)screenHeight / 2.0f };
    float edgeMargin = 40.0f;

    for (const auto& playerData : otherPlayersData) {
        Vector3 targetPos = playerData.first;
        const char* playerName = playerData.second.c_str();

        float distance = Vector3Distance(camera.position, targetPos);
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

void World::CheckShipCollisions(BoundingBox asteroidBox, Camera3D camera)
{
    BoundingBox PlayerBox = Models::GetWorldBoundingBox(Models::ShipBoxLocal, PlayerShip.Position, PlayerShip.Rotation);

    // check player-asteroid collisions
    if(CheckCollisionBoxes(PlayerBox, asteroidBox))
    {
        Vector3 hitPosition = PlayerShip.Position;
        PlayerShip.Respawn();
        Sounds::PlayHurt(hitPosition, PlayerShip.Position, camera);
        Net.HandlePlayerCollision();
    }
}

void World::CheckLaserCollisions(BoundingBox asteroidBox, int asteroidId, Camera3D camera)
{
    if(!PlayerShip.isFiring){
        return;
    }

    Vector3 start = PlayerShip.Position;
    Vector3 direction = PlayerShip.GetForwardVector();
    Ray laser = { start, direction };

    // Calculate bounding boxes for asteroids, and check collisions
    if(GetRayCollisionBox(laser, asteroidBox).hit)
    {
        Vector3 asteroidPosition = { 0.0f, 0.0f, 0.0f };
        Matrix asteroidRotation = MatrixIdentity();
        if (Net.GetAsteroidSpatial(asteroidId, &asteroidPosition, &asteroidRotation))
        {
            Sounds::PlayExplosion(asteroidPosition, PlayerShip.Position, camera);
        }

        Net.HandleDestroyAsteroid(Net.GetLocalPlayerId(), asteroidId);
        PlayerShip.isFiring = false;
    }
}

void World::CheckCollisions(Camera3D camera)
{
    for(int i = 0; i < Net.GetMaxAsteroids(); i++)
    {
        CheckShipCollisions(AsteroidBoundingBoxes[i], camera);
        CheckLaserCollisions(AsteroidBoundingBoxes[i], i, camera);
    }
}

