#pragma once
#include "raylib/raylib.h"
#include "networking/NetConstants.h"
#include <string>

enum class MenuChoice { Host, Join };
enum class InputFocus { Name, Address, None };
enum class MenuAction { None, StartHost, StartJoin };

class MenuScreen
{
public:
    MenuScreen();

    void Init(const std::string& defaultAddress);
    MenuAction Update();
    void Draw(const std::string& hostAddress);
    void DrawConnecting();
    void SetStatusMessage(const char* msg);

    const char* GetPlayerName() const { return playerNameBuffer; }
    const char* GetAddress() const { return addressBuffer; }
    bool IsJoinMode() const { return joinMode; }

private:
    void HandleTextInput(char* buffer, int* length, int maxLength);

    static const int MAX_ADDRESS_LENGTH = 63;
    char addressBuffer[MAX_ADDRESS_LENGTH + 1];
    int addressLength;
    
    char playerNameBuffer[MAX_PLAYER_NAME_LENGTH];
    int playerNameLength;
    
    char statusMessage[128];

    MenuChoice selection;
    InputFocus inputFocus;
    bool joinMode;
};