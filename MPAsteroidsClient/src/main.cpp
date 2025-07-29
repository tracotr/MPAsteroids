#include "include/raylib/raylib.h"
#include "include/raylib/rcamera.h"
#include "include/raylib/raymath.h"

#include "include/Game.h"
#include "include/Models.h"
#include "include/World.h"
#include "include/Player.h"

#include "include/NetClient.h"
#include "include/networking/NetConstants.h"

#include <stdio.h>

const Vector3 CAMERA_OFFSET = (Vector3){ 0.0f, 2.0f, 5.0f };
const Vector3 CAMERA_UP = (Vector3){ 0.0f, 1.0f, 0.0f };

void UpdateCameraCustom(Camera* camera, Player* playerShip);

int main()
{
    // raylib window setup
    SetConfigFlags(FLAG_MSAA_4X_HINT); 
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "MPAsteroid");
    SetTargetFPS(60);
    DisableCursor();

    // load all models
    Models::Init();

    // load camera for client
    Camera3D camera = { 0 };
    camera.position = CAMERA_OFFSET;                    // Camera position perspective
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };      // Camera looking at point
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };          // Camera up vector (rotation towards target)
    camera.fovy = 90.0f;                                // Camera field-of-view Y
    camera.projection = CAMERA_PERSPECTIVE;             // Camera type

    // spawn in the world
    World& world = World::Create();
    world.Reset();

    NetConnect("127.0.0.1");

    while (!WindowShouldClose())
    {
        // Updating
        // -------------
        world.Update(GetFrameTime());
        NetUpdate(GetTime(), GetFrameTime());

        UpdateCameraCustom(&camera, &world.PlayerShip);

        // Drawing
        // -------------
        BeginDrawing();

            ClearBackground(BLACK);
            
            BeginMode3D(camera);
          
                world.DrawModels();

            EndMode3D();

            world.DrawUI(camera);

        EndDrawing();
    }
}

void UpdateCameraCustom(Camera* camera, Player* playerShip)
{
    // Get vectors for camera rotation
    Vector3 forwardVector = Vector3Transform(CAMERA_OFFSET, playerShip->Rotation);
    Vector3 offsetVector = Vector3Add(forwardVector, playerShip->Position);
    Vector3 upVector = Vector3Transform(CAMERA_UP, playerShip->Rotation);

    // apply new camera position and targets
    camera->position = Vector3Lerp(camera->position, offsetVector, 0.1f);
    camera->target = playerShip->Position;
    camera->up = upVector;

    // apply fov of camera based off player speed
    float speedRatio = Clamp(Vector3LengthSqr(playerShip->Velocity) / playerShip->MaxSpeed, 0.0f, 1.0f);
    float targetFOV = Lerp(90, 110, speedRatio);
    camera->fovy = Lerp(camera->fovy, targetFOV, 0.1f);
}