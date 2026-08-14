#pragma once
#include "raylib/raylib.h"
#include "ServerHost.h"
#include "MenuScreen.h"
#include <string>

enum class AppState
{
    Menu,
    Connecting,
    Playing
};

class GameApp
{
public:
    GameApp();
    
    void Initialize();
    void RunLoop();
    void Shutdown();

private:
    void ProcessMenu();
    void ProcessConnecting();
    void ProcessPlaying();
    void UpdateCamera();

    AppState state;
    Camera3D camera;
    ServerHost hostedServer;
    MenuScreen menu;

    std::string hostAddress;
    double connectStartTime;

    const Vector3 CAMERA_OFFSET = { 0.0f, 2.0f, 5.0f };
    const Vector3 CAMERA_UP = { 0.0f, 1.0f, 0.0f };
    const double CONNECT_TIMEOUT_SECONDS = 6.0;
};