#include "include/GameApp.h"
#include "include/Game.h"
#include "include/Models.h"
#include "include/Sounds.h"
#include "include/World.h"
#include "include/raylib/rcamera.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

GameApp* GameApp::instance = nullptr;

namespace
{
    const char* const kNameAdjectives[] = {
        "Swift", "Rogue", "Lunar", "Solar", "Void", "Nova", "Astro", "Comet",
        "Nebula", "Quasar", "Orbit", "Cosmic", "Vector", "Photon", "Ion", "Pulsar"
    };

    const char* const kNameNouns[] = {
        "Pilot", "Hawk", "Drift", "Racer", "Falcon", "Wing", "Runner", "Ace",
        "Scout", "Ranger", "Nomad", "Raider", "Flyer", "Dart", "Blaze", "Comet"
    };

    // Match the render resolution to the canvas's actual displayed size so the
    // view isn't stretched when the embedding page sizes the canvas.
    void SyncCanvasSize()
    {
        double cssWidth = 0.0, cssHeight = 0.0;
        if (emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight) != EMSCRIPTEN_RESULT_SUCCESS)
            return;

        int width = (int)cssWidth;
        int height = (int)cssHeight;
        if (width <= 0 || height <= 0) return;

        if (width != GetScreenWidth() || height != GetScreenHeight())
            SetWindowSize(width, height);
    }

    EM_BOOL OnBrowserResize(int, const EmscriptenUiEvent*, void*)
    {
        SyncCanvasSize();
        return EM_FALSE;
    }

    // Fits within MAX_PLAYER_NAME_LENGTH (16 incl. terminator).
    std::string MakeRandomName()
    {
        const int adjectiveCount = sizeof(kNameAdjectives) / sizeof(kNameAdjectives[0]);
        const int nounCount = sizeof(kNameNouns) / sizeof(kNameNouns[0]);

        char buffer[MAX_PLAYER_NAME_LENGTH];
        std::snprintf(buffer, sizeof(buffer), "%s%s%02d",
                      kNameAdjectives[GetRandomValue(0, adjectiveCount - 1)],
                      kNameNouns[GetRandomValue(0, nounCount - 1)],
                      GetRandomValue(0, 99));
        return std::string(buffer);
    }
}

GameApp::GameApp()
    : state(AppState::Connecting), connectStartTime(0.0), nextRetryTime(0.0), hasPlayedBefore(false)
{
    instance = this;
}

GameApp* GameApp::GetInstance()
{
    return instance;
}

void GameApp::Initialize()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "MPAsteroids");

    SetRandomSeed((unsigned int)std::time(nullptr));

    Sounds::Init();
    Models::Init();

    camera = { 0 };
    camera.position = CAMERA_OFFSET;
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = CAMERA_UP;
    camera.fovy = 90.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    World::Create().Reset();

    SyncCanvasSize();
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, OnBrowserResize);

    playerName = MakeRandomName();
    BeginConnectAttempt();
}

void GameApp::BeginConnectAttempt()
{
    state = AppState::Connecting;
    connectStartTime = GetTime();

    if (!Net.NetConnect(SERVER_HOST, playerName.c_str()))
        nextRetryTime = GetTime() + RETRY_DELAY_SECONDS;
    else
        nextRetryTime = 0.0;
}

void GameApp::Tick()
{
    switch (state)
    {
        case AppState::Connecting:
            ProcessConnecting();
            break;
        case AppState::Playing:
            ProcessPlaying();
            break;
    }
}

void GameApp::Shutdown()
{
    Net.NetDisconnect();
    Sounds::Shutdown();
    CloseWindow();
}

void GameApp::ProcessConnecting()
{
    Net.NetUpdate(GetTime(), GetFrameTime());

    if (Net.GetLocalPlayerId() != -1)
    {
        World::Create().Reset();
        state = AppState::Playing;
        hasPlayedBefore = true;
    }
    else
    {
        bool socketDead = Net.GetStatus() == NetStatus::Disconnected;
        bool timedOut = GetTime() - connectStartTime > CONNECT_TIMEOUT_SECONDS;

        if (socketDead || timedOut)
        {
            if (nextRetryTime == 0.0)
                nextRetryTime = GetTime() + RETRY_DELAY_SECONDS;
            else if (GetTime() >= nextRetryTime)
                BeginConnectAttempt();
        }
    }

    BeginDrawing();
        ClearBackground(BLACK);
        DrawConnectingScreen(hasPlayedBefore ? "Reconnecting..." : "Connecting...");
    EndDrawing();
}

void GameApp::DrawConnectingScreen(const char* message)
{
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    const char* title = "MP ASTEROIDS";
    int titleWidth = MeasureText(title, 40);
    DrawText(title, (screenWidth - titleWidth) / 2, screenHeight / 2 - 70, 40, RAYWHITE);

    int messageWidth = MeasureText(message, 24);
    DrawText(message, (screenWidth - messageWidth) / 2, screenHeight / 2, 24, GRAY);

    const char* nameLine = TextFormat("Playing as %s", playerName.c_str());
    int nameWidth = MeasureText(nameLine, 18);
    DrawText(nameLine, (screenWidth - nameWidth) / 2, screenHeight / 2 + 40, 18, DARKGRAY);
}

void GameApp::ProcessPlaying()
{
    if (Net.GetStatus() != NetStatus::Connected)
    {
        nextRetryTime = GetTime() + RETRY_DELAY_SECONDS;
        state = AppState::Connecting;
        connectStartTime = GetTime();
        return;
    }

    // A backgrounded tab starves requestAnimationFrame, so the first frame back
    // can carry a multi-second delta that would teleport the ship and its
    // projectiles across the map. Cap it to a sane step.
    float delta = GetFrameTime();
    if (delta > MAX_FRAME_DELTA) delta = MAX_FRAME_DELTA;

    World& world = World::Create();
    world.Update(delta);
    UpdateCamera();

    BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        world.Draw();
        EndMode3D();
        world.DrawUI();
    EndDrawing();
}

void GameApp::UpdateCamera()
{
    Vector3 forwardVector = Vector3Transform(CAMERA_OFFSET, PlayerShip.Rotation);
    Vector3 offsetVector = Vector3Add(forwardVector, PlayerShip.Position);
    Vector3 upVector = Vector3Transform(CAMERA_UP, PlayerShip.Rotation);

    camera.position = Vector3Lerp(camera.position, offsetVector, 0.1f);
    camera.target = PlayerShip.Position;
    camera.up = upVector;

    float speedRatio = Clamp(Vector3LengthSqr(PlayerShip.Velocity) / PlayerShip.MaxSpeed, 0.0f, 1.0f);
    float targetFOV = Lerp(90, 110, speedRatio);
    camera.fovy = Lerp(camera.fovy, targetFOV, 0.1f);
}
