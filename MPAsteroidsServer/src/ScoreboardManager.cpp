#include "include/ScoreboardManager.h"

// resets score of given id
void ScoreboardManager::ResetScoreId(int playerId)
{
    Scoreboard[playerId] = 0;
}

// resets all scores to 0
void ScoreboardManager::ResetScoreboard()
{
    for(int i = 0; i < MAX_PLAYERS; i++)
    {
        Scoreboard[i] = 0;
    }
}

// adds score to given id
void ScoreboardManager::AddScoreId(int playerId, int scoreAmount)
{
    Scoreboard[playerId] += scoreAmount;
}

void ScoreboardManager::UpdateScoreboard(PlayerInfo (&players)[MAX_PLAYERS])
{
    ScoreboardPacket scoreboardBuffer;
    scoreboardBuffer.Command = NetworkCommands::UpdateScoreboard;
    memcpy(scoreboardBuffer.Scoreboard, Scoreboard, sizeof(Scoreboard));
    // create packet and send
    ENetPacket* packet = enet_packet_create(&scoreboardBuffer, sizeof(scoreboardBuffer), 0);
    NetworkUtil::SendPacketToAllBut(packet, players, -1, 1);
}
