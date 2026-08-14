#include "include/MenuScreen.h"
#include "include/Game.h"
#include <cstring>
#include <cstdio>

MenuScreen::MenuScreen() 
    : addressLength(0), playerNameLength(0), selection(MenuChoice::Host), 
      inputFocus(InputFocus::Name), joinMode(false)
{
    statusMessage[0] = '\0';
    std::strncpy(playerNameBuffer, "Player", MAX_PLAYER_NAME_LENGTH - 1);
    playerNameBuffer[MAX_PLAYER_NAME_LENGTH - 1] = '\0';
    playerNameLength = static_cast<int>(std::strlen(playerNameBuffer));
    addressBuffer[0] = '\0';
}

void MenuScreen::Init(const std::string& defaultAddress)
{
    std::strncpy(addressBuffer, defaultAddress.c_str(), MAX_ADDRESS_LENGTH);
    addressBuffer[MAX_ADDRESS_LENGTH] = '\0';
    addressLength = static_cast<int>(std::strlen(addressBuffer));
}

void MenuScreen::SetStatusMessage(const char* msg)
{
    std::snprintf(statusMessage, sizeof(statusMessage), "%s", msg);
}

void MenuScreen::HandleTextInput(char* buffer, int* length, int maxLength)
{
    int key = GetCharPressed();
    while (key > 0)
    {
        bool isPrintableChar = (key >= 32) && (key <= 125);
        if (isPrintableChar && *length < maxLength)
        {
            buffer[*length] = (char)key;
            (*length)++;
            buffer[*length] = '\0';
        }
        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE) && *length > 0)
    {
        (*length)--;
        buffer[*length] = '\0';
    }
}

MenuAction MenuScreen::Update()
{
    // Clear name on TAB
    if (IsKeyPressed(KEY_TAB))
    {
        playerNameLength = 0;
        playerNameBuffer[0] = '\0';
    }

    // Input Box Rectangles
    Rectangle nameBox = { 420.0f, 165.0f, 300.0f, 32.0f };
    Rectangle addressBox = { 350.0f, 320.0f, 580.0f, 40.0f };
    Rectangle hostButton = { 370.0f, 220.0f, 220.0f, 44.0f };
    Rectangle joinButton = { 690.0f, 220.0f, 220.0f, 44.0f };

    Vector2 mouse = GetMousePosition();

    // Mouse Interactions
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
    {
        if (CheckCollisionPointRec(mouse, nameBox)) inputFocus = InputFocus::Name;
        else if (CheckCollisionPointRec(mouse, addressBox)) inputFocus = InputFocus::Address;
        else inputFocus = InputFocus::None;

        if (CheckCollisionPointRec(mouse, hostButton))
        {
            selection = MenuChoice::Host;
            statusMessage[0] = '\0';
            return MenuAction::StartHost;
        }
        else if (CheckCollisionPointRec(mouse, joinButton))
        {
            selection = MenuChoice::Join;
            joinMode = true;
        }
    }

    // Keyboard Focus
    if (inputFocus == InputFocus::Name)
    {
        HandleTextInput(playerNameBuffer, &playerNameLength, MAX_PLAYER_NAME_LENGTH - 1);
    }
    else if (inputFocus == InputFocus::Address)
    {
        HandleTextInput(addressBuffer, &addressLength, MAX_ADDRESS_LENGTH);
    }

    // Hotkeys
    if (IsKeyPressed(KEY_H)) selection = MenuChoice::Host;
    if (IsKeyPressed(KEY_J)) selection = MenuChoice::Join;

    if (IsKeyPressed(KEY_ENTER))
    {
        if (selection == MenuChoice::Host) return MenuAction::StartHost;
        if (selection == MenuChoice::Join && addressLength > 0) return MenuAction::StartJoin;
    }

    return MenuAction::None;
}

void MenuScreen::Draw(const std::string& hostAddress)
{
    const char* title = "MP ASTEROIDS";
    int titleWidth = MeasureText(title, 40);
    DrawText(title, (WINDOW_WIDTH - titleWidth) / 2, 80, 40, RAYWHITE);
    
    DrawText("Player name:", 290, 175, 18, RAYWHITE);
    Rectangle nameBox = { 420.0f, 165.0f, 300.0f, 32.0f };
    DrawRectangleRec(nameBox, Fade(RAYWHITE, 0.1f));
    DrawRectangleLinesEx(nameBox, 2, inputFocus == InputFocus::Name ? GREEN : RAYWHITE);
    DrawText(playerNameBuffer, 430, 171, 20, GREEN);
    
    if (inputFocus == InputFocus::Name && ((int)(GetTime() * 2.0)) % 2 == 0)
    {
        int textWidth = MeasureText(playerNameBuffer, 20);
        DrawText("_", 430 + textWidth, 171, 20, GREEN);
    }

    Rectangle hostButton = { 370.0f, 220.0f, 220.0f, 44.0f };
    Rectangle joinButton = { 690.0f, 220.0f, 220.0f, 44.0f };
    Color hostColor = (selection == MenuChoice::Host) ? SKYBLUE : DARKGRAY;
    Color joinColor = (selection == MenuChoice::Join) ? SKYBLUE : DARKGRAY;

    DrawRectangleRec(hostButton, hostColor);
    DrawRectangleRec(joinButton, joinColor);
    DrawRectangleLinesEx(hostButton, 2, RAYWHITE);
    DrawRectangleLinesEx(joinButton, 2, RAYWHITE);
    
    DrawText("Host Game", (int)hostButton.x + 58, (int)hostButton.y + 12, 24, RAYWHITE);
    DrawText("Join Game", (int)joinButton.x + 60, (int)joinButton.y + 12, 24, RAYWHITE);

    if (joinMode || selection == MenuChoice::Join)
    {
        DrawText("Server address:", 350, 290, 20, RAYWHITE);
        Rectangle box = { 350.0f, 320.0f, 580.0f, 40.0f };
        DrawRectangleRec(box, Fade(RAYWHITE, 0.1f));
        DrawRectangleLinesEx(box, 2, inputFocus == InputFocus::Address ? GREEN : RAYWHITE);
        DrawText(addressBuffer, 360, 331, 20, GREEN);

        if (inputFocus == InputFocus::Address && ((int)(GetTime() * 2.0)) % 2 == 0)
        {
            int textWidth = MeasureText(addressBuffer, 20);
            DrawText("_", 360 + textWidth, 331, 20, GREEN);
        }

        DrawText("Press ENTER to connect to the host.", 350, 380, 18, GRAY);
    }
    else
    {
        DrawText("Host on this machine and let friends connect to you.", 290, 290, 18, GRAY);
        DrawText(TextFormat("Your LAN IP: %s:%d", hostAddress.c_str(), SERVER_PORT), 290, 330, 20, GREEN);
        DrawText("Press ENTER to host a game or use H/J to switch modes.", 290, 365, 18, GRAY);
        DrawText("Use a VPN or port-forward UDP 25665 if playing over the internet.", 290, 395, 18, GRAY);
    }

    if (statusMessage[0] != '\0')
    {
        int msgWidth = MeasureText(statusMessage, 18);
        DrawText(statusMessage, (WINDOW_WIDTH - msgWidth) / 2, 470, 18, RED);
    }
}

void MenuScreen::DrawConnecting()
{
    const char* text = TextFormat("Connecting to %s ...", addressBuffer);
    int textWidth = MeasureText(text, 24);
    DrawText(text, (WINDOW_WIDTH - textWidth) / 2, WINDOW_HEIGHT / 2 - 12, 24, RAYWHITE);
}