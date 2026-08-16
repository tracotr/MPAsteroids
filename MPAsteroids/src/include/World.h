#pragma once
#include "Entity.h"
#include "Player.h"
#include "Models.h"
#include "NetClient.h"
#include "Sounds.h"
#include "networking/NetConstants.h"

struct Projectile {
    bool active = false;
    Vector3 position;
    Vector3 velocity;
    float lifeTime;
};

class World {
public:
    static World* Instance;
    static World& Create();
    static void Destroy();

    void Reset();
    void Update(double delta);
    void Draw();
    void DrawPlayerModels();
    
    void DrawAsteroidModels();
    void DrawUI();
    
    void UpdateProjectiles(double delta);
    
    void FireProjectile(Vector3 position, Vector3 velocity);
    
    void DrawProjectiles();
    void CheckProjectileCollisions();
    void CreateAsteroidCollision();
    void CheckShipCollisions(BoundingBox asteroidBox);
    void CheckCollisions();

    Projectile Projectiles[MAX_PROJECTILES];
    BoundingBox AsteroidBoundingBoxes[MAX_ASTEROIDS];
};