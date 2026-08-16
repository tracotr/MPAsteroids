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

    float nameBoxWidth = 300.0f;
    nameBox = { (WINDOW_WIDTH - nameBoxWidth) / 2.0f, 160.0f, nameBoxWidth, 32.0f };
    
    float btnWidth = 220.0f;
    hostButton = { (WINDOW_WIDTH - btnWidth) / 2.0f, 220.0f, btnWidth, 44.0f };
    joinButton = { (WINDOW_WIDTH - btnWidth) / 2.0f, 280.0f, btnWidth, 44.0f };
    
    float addressBoxWidth = 580.0f;
    addressBox = { (WINDOW_WIDTH - addressBoxWidth) / 2.0f, 380.0f, addressBoxWidth, 40.0f };
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
    if (IsKeyPressed(KEY_TAB))
    {
        playerNameLength = 0;
        playerNameBuffer[0] = '\0';
    }

    Vector2 mouse = GetMousePosition();

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

    if (inputFocus == InputFocus::Name)
    {
        HandleTextInput(playerNameBuffer, &playerNameLength, MAX_PLAYER_NAME_LENGTH - 1);
    }
    else if (inputFocus == InputFocus::Address)
    {
        HandleTextInput(addressBuffer, &addressLength, MAX_ADDRESS_LENGTH);
    }

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
    
    const char* nameLabel = "Player name:";
    int nameLabelWidth = MeasureText(nameLabel, 18);
    DrawText(nameLabel, (WINDOW_WIDTH - nameLabelWidth) / 2, 135, 18, RAYWHITE);
    
    DrawRectangleRec(nameBox, Fade(RAYWHITE, 0.1f));
    DrawRectangleLinesEx(nameBox, 2, inputFocus == InputFocus::Name ? GREEN : RAYWHITE);
    DrawText(playerNameBuffer, (int)nameBox.x + 10, (int)nameBox.y + 6, 20, GREEN);
    
    if (inputFocus == InputFocus::Name && ((int)(GetTime() * 2.0)) % 2 == 0)
    {
        int textWidth = MeasureText(playerNameBuffer, 20);
        DrawText("_", (int)nameBox.x + 10 + textWidth, (int)nameBox.y + 6, 20, GREEN);
    }

    Color hostColor = (selection == MenuChoice::Host) ? SKYBLUE : DARKGRAY;
    Color joinColor = (selection == MenuChoice::Join) ? SKYBLUE : DARKGRAY;

    DrawRectangleRec(hostButton, hostColor);
    DrawRectangleRec(joinButton, joinColor);
    DrawRectangleLinesEx(hostButton, 2, RAYWHITE);
    DrawRectangleLinesEx(joinButton, 2, RAYWHITE);
    
    int hostTextWidth = MeasureText("Host Game", 24);
    int joinTextWidth = MeasureText("Join Game", 24);
    DrawText("Host Game", (int)hostButton.x + (hostButton.width - hostTextWidth) / 2, (int)hostButton.y + 10, 24, RAYWHITE);
    DrawText("Join Game", (int)joinButton.x + (joinButton.width - joinTextWidth) / 2, (int)joinButton.y + 10, 24, RAYWHITE);

    if (joinMode || selection == MenuChoice::Join)
    {
        const char* addrLabel = "Server address:";
        int addrLabelWidth = MeasureText(addrLabel, 20);
        DrawText(addrLabel, (WINDOW_WIDTH - addrLabelWidth) / 2, 350, 20, RAYWHITE);
        
        DrawRectangleRec(addressBox, Fade(RAYWHITE, 0.1f));
        DrawRectangleLinesEx(addressBox, 2, inputFocus == InputFocus::Address ? GREEN : RAYWHITE);
        DrawText(addressBuffer, (int)addressBox.x + 10, (int)addressBox.y + 10, 20, GREEN);

        if (inputFocus == InputFocus::Address && ((int)(GetTime() * 2.0)) % 2 == 0)
        {
            int textWidth = MeasureText(addressBuffer, 20);
            DrawText("_", (int)addressBox.x + 10 + textWidth, (int)addressBox.y + 10, 20, GREEN);
        }

        const char* joinHint = "Press ENTER to connect to the host.";
        int joinHintWidth = MeasureText(joinHint, 18);
        DrawText(joinHint, (WINDOW_WIDTH - joinHintWidth) / 2, 440, 18, GRAY);
    }
    else
    {
        const char* hostTxt1 = "Host on this machine and let friends connect to you.";
        const char* hostTxt2 = TextFormat("Your LAN IP: %s:%d", hostAddress.c_str(), SERVER_PORT);
        const char* hostTxt3 = "Press ENTER to host a game or use H/J to switch modes.";
        const char* hostTxt4 = "Use a VPN or port-forward UDP 25665 if playing over the internet.";

        DrawText(hostTxt1, (WINDOW_WIDTH - MeasureText(hostTxt1, 18)) / 2, 340, 18, GRAY);
        DrawText(hostTxt2, (WINDOW_WIDTH - MeasureText(hostTxt2, 20)) / 2, 370, 20, GREEN);
        DrawText(hostTxt3, (WINDOW_WIDTH - MeasureText(hostTxt3, 18)) / 2, 405, 18, GRAY);
        DrawText(hostTxt4, (WINDOW_WIDTH - MeasureText(hostTxt4, 18)) / 2, 435, 18, GRAY);
    }

    if (statusMessage[0] != '\0')
    {
        int msgWidth = MeasureText(statusMessage, 18);
        DrawText(statusMessage, (WINDOW_WIDTH - msgWidth) / 2, 480, 18, RED);
    }
}

void MenuScreen::DrawConnecting()
{
    const char* text = TextFormat("Connecting to %s ...", addressBuffer);
    int textWidth = MeasureText(text, 24);
    DrawText(text, (WINDOW_WIDTH - textWidth) / 2, WINDOW_HEIGHT / 2 - 12, 24, RAYWHITE);
}