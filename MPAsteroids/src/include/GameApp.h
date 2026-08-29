#pragma once
#include "raylib/raylib.h"
#include "NetClient.h"
#include "Player.h"
#include <string>

enum class AppState {
    Connecting,
    Playing
};

class GameApp {
public:
    GameApp();

    void Initialize();
    void Tick();
    void Shutdown();

    static GameApp* GetInstance();
    Camera3D GetCamera() const { return camera; }

    NetClient& GetNet() { return Net; }

    // Whether the browser has actually given us pointer lock. Not IsCursorHidden():
    // on web DisableCursor() only requests the lock and never sets that flag.
    bool IsMouseCaptured() const { return mouseCaptured; }
    Player& GetPlayer() { return PlayerShip; }

private:
    void ProcessConnecting();
    void ProcessPlaying();
    void UpdateCamera();
    void UpdateMouseCapture();
    void DrawConnectingScreen(const char* message);
    void BeginConnectAttempt();

    AppState state;
    Camera3D camera;

    NetClient Net;
    Player PlayerShip;

    static GameApp* instance;
    std::string playerName;
    double connectStartTime;
    double nextRetryTime;
    bool hasPlayedBefore;
    bool mouseCaptured = false;

    const Vector3 CAMERA_OFFSET = { 0.0f, 2.0f, 5.0f };
    const Vector3 CAMERA_UP = { 0.0f, 1.0f, 0.0f };
    const double CONNECT_TIMEOUT_SECONDS = 10.0;
    const double RETRY_DELAY_SECONDS = 2.0;
    static constexpr float MAX_FRAME_DELTA = 0.1f;
};
