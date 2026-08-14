#include "include/GameApp.h"
#include "include/Game.h"
#include "include/Models.h"
#include "include/Sounds.h"
#include "include/World.h"
#include "include/NetUtils.h"
#include "include/raylib/rcamera.h"
#include <cstdio>

GameApp::GameApp() : state(AppState::Menu), connectStartTime(0.0)
{
}

void GameApp::Initialize()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "MPAsteroids");
    SetTargetFPS(60);

    Sounds::Init();
    Models::Init();

    camera = { 0 };
    camera.position = CAMERA_OFFSET;
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = CAMERA_UP;
    camera.fovy = 90.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    World::Create().Reset();

    hostAddress = NetUtils::GetLocalIPv4();
    menu.Init(hostAddress);
}

void GameApp::RunLoop()
{
    while (!WindowShouldClose())
    {
        switch (state)
        {
            case AppState::Menu:
                ProcessMenu();
                break;
            case AppState::Connecting:
                ProcessConnecting();
                break;
            case AppState::Playing:
                ProcessPlaying();
                break;
        }
    }
}

void GameApp::Shutdown()
{
    hostedServer.Stop();
    Sounds::Shutdown();
    CloseWindow();
}

void GameApp::ProcessMenu()
{
    MenuAction action = menu.Update();
    World& world = World::Create();

    if (action == MenuAction::StartHost)
    {
        if (hostedServer.Start())
        {
            if (world.Net.NetConnect("127.0.0.1", menu.GetPlayerName()))
            {
                world.Net.BeginHostedSession(menu.GetPlayerName());
                connectStartTime = GetTime();
                state = AppState::Playing;
            }
            else
            {
                hostedServer.Stop();
                menu.SetStatusMessage("Couldn't start local host session.");
            }
        }
        else
        {
            menu.SetStatusMessage("Couldn't start the server thread.");
        }
    }
    else if (action == MenuAction::StartJoin)
    {
        if (world.Net.NetConnect(menu.GetAddress(), menu.GetPlayerName()))
        {
            connectStartTime = GetTime();
            menu.SetStatusMessage("");
            state = AppState::Connecting;
        }
        else
        {
            menu.SetStatusMessage("Couldn't start networking - try again.");
        }
    }

    BeginDrawing();
        ClearBackground(BLACK);
        menu.Draw(hostAddress);
    EndDrawing();
}

void GameApp::ProcessConnecting()
{
    World& world = World::Create();
    world.Net.NetUpdate(GetTime(), GetFrameTime());

    if (world.Net.GetLocalPlayerId() != -1)
    {
        DisableCursor();
        state = AppState::Playing;
    }
    else if (GetTime() - connectStartTime > CONNECT_TIMEOUT_SECONDS)
    {
        char errorMsg[256];
        snprintf(errorMsg, sizeof(errorMsg), "Couldn't reach %s - check the address and try again.", menu.GetAddress());
        menu.SetStatusMessage(errorMsg);
        
        if (hostedServer.IsRunning())
            hostedServer.Stop();
            
        state = AppState::Menu;
    }

    BeginDrawing();
        ClearBackground(BLACK);
        menu.DrawConnecting();
    EndDrawing();
}

void GameApp::ProcessPlaying()
{
    World& world = World::Create();
    world.Update(GetFrameTime());
    UpdateCamera();

    BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        world.Draw();
        EndMode3D();
        world.DrawUI(camera);
    EndDrawing();
}

void GameApp::UpdateCamera()
{
    Player& playerShip = World::Create().PlayerShip;
    Vector3 forwardVector = Vector3Transform(CAMERA_OFFSET, playerShip.Rotation);
    Vector3 offsetVector = Vector3Add(forwardVector, playerShip.Position);
    Vector3 upVector = Vector3Transform(CAMERA_UP, playerShip.Rotation);

    camera.position = Vector3Lerp(camera.position, offsetVector, 0.1f);
    camera.target = playerShip.Position;
    camera.up = upVector;

    float speedRatio = Clamp(Vector3LengthSqr(playerShip.Velocity) / playerShip.MaxSpeed, 0.0f, 1.0f);
    float targetFOV = Lerp(90, 110, speedRatio);
    camera.fovy = Lerp(camera.fovy, targetFOV, 0.1f);
}