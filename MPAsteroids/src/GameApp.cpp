#include "include/GameApp.h"
#include "include/Game.h"
#include "include/Models.h"
#include "include/Names.h"
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
    // Without this the framebuffer keeps its initial size and the embedding page stretches it.
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
    Names::Init();

    camera = { 0 };
    camera.position = CAMERA_OFFSET;
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = CAMERA_UP;
    camera.fovy = 90.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    World::Create().Reset();

    SyncCanvasSize();
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, EM_FALSE, OnBrowserResize);

    playerName = Names::MakeRandom();
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
        if (mouseCaptured) { EnableCursor(); mouseCaptured = false; }

        nextRetryTime = GetTime() + RETRY_DELAY_SECONDS;
        state = AppState::Connecting;
        connectStartTime = GetTime();
        return;
    }

    // A backgrounded tab stops receiving frames.
    float delta = GetFrameTime();
    if (delta > MAX_FRAME_DELTA) delta = MAX_FRAME_DELTA;

    UpdateMouseCapture();

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

// Browsers only hand over pointer lock from inside a real user gesture.
void GameApp::UpdateMouseCapture()
{
    // The browser is the authority here.
    EmscriptenPointerlockChangeEvent status = {};
    mouseCaptured = (emscripten_get_pointerlock_status(&status) == EMSCRIPTEN_RESULT_SUCCESS) && status.isActive;

    if (!mouseCaptured && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        DisableCursor();
}

void GameApp::UpdateCamera()
{
    Vector3 forwardVector = Vector3Transform(CAMERA_OFFSET, PlayerShip.Rotation);
    Vector3 offsetVector = Vector3Add(forwardVector, PlayerShip.Position);
    Vector3 upVector = Vector3Transform(CAMERA_UP, PlayerShip.Rotation);

    camera.position = Vector3Lerp(camera.position, offsetVector, 0.1f);
    camera.target = PlayerShip.Position;
    camera.up = upVector;

    // A real fraction of the ship's real top speed.
    const float topSpeed = Net.GetStats().TopSpeed;
    float speedRatio = Clamp(Vector3Length(PlayerShip.Velocity) / (topSpeed > 0.0f ? topSpeed : 1.0f), 0.0f, 1.0f);
    float targetFOV = Lerp(90, 110, speedRatio);
    camera.fovy = Lerp(camera.fovy, targetFOV, 0.1f);
}
