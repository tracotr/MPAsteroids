#pragma once
#include "networking/NetCommon.h"
#include "PlayerInfo.h"
#include "NetworkUtil.h"

class ScoreboardManager
{
private:
    int Scoreboard[MAX_PLAYERS] = { 0 };

public:
    void ResetScoreId(int playerId);
    void ResetScoreboard();
    void AddScoreId(int playerId, int scoreAmount);
    void UpdateScoreboard(PlayerInfo (&players)[MAX_PLAYERS]);

    int (&GetScoreboard())[MAX_PLAYERS] { return Scoreboard; };
};
