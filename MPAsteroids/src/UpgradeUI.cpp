#include "include/UpgradeUI.h"

#include "include/Upgrades.h"

#include <cstring>

namespace
{
    const int CARD_WIDTH = 170;
    const int CARD_HEIGHT = 40;
    const int CARD_GAP = 10;

    const int TITLE_SIZE = 11;
    const int BODY_SIZE = 10;
    const int CARD_PADDING = 8;
    const int BADGE_SIZE = 20;

    // Readable against the skybox without blacking out the game behind it, since
    // choosing does not pause anything.
    const Color CARD_FILL = { 12, 16, 26, 225 };
    const Color CARD_EDGE = { 90, 140, 200, 255 };
    const Color MILESTONE_EDGE = { 235, 180, 70, 255 };

    // The two bars sit centred on the bottom edge, with the cards stacked above
    // them, since both want the middle and the bars are always there.
    const int BAR_WIDTH = 400;
    const int BAR_HEIGHT = 20;
    const int BAR_GAP = 8;
    const int BAR_BOTTOM_MARGIN = 12;

    // Breaks text on spaces to fit a pixel width. raylib has no wrapping for the
    // plain DrawText path, and the descriptions are written as sentences.
    int WrapText(const char* text, int fontSize, int maxWidth, char lines[][64], int maxLines)
    {
        int lineCount = 0;
        char current[64] = { 0 };
        int currentLength = 0;

        const char* word = text;
        while (word != nullptr && *word != '\0' && lineCount < maxLines)
        {
            const char* space = strchr(word, ' ');
            int wordLength = (space != nullptr) ? (int)(space - word) : (int)strlen(word);
            if (wordLength > 62) wordLength = 62;

            char candidate[64] = { 0 };
            if (currentLength > 0)
            {
                memcpy(candidate, current, currentLength);
                candidate[currentLength] = ' ';
                memcpy(candidate + currentLength + 1, word, wordLength);
            }
            else
            {
                memcpy(candidate, word, wordLength);
            }

            if (MeasureText(candidate, fontSize) <= maxWidth || currentLength == 0)
            {
                memcpy(current, candidate, sizeof(current));
                currentLength = (int)strlen(current);
            }
            else
            {
                memcpy(lines[lineCount++], current, sizeof(current));
                memset(current, 0, sizeof(current));
                memcpy(current, word, wordLength);
                currentLength = wordLength;
            }

            word = (space != nullptr) ? space + 1 : nullptr;
        }

        if (currentLength > 0 && lineCount < maxLines)
            memcpy(lines[lineCount++], current, sizeof(current));

        return lineCount;
    }

    void DrawBar(int x, int y, int width, int height, float fraction, Color fill, const char* label)
    {
        if (fraction < 0.0f) fraction = 0.0f;
        if (fraction > 1.0f) fraction = 1.0f;

        DrawRectangle(x, y, width, height, (Color){ 0, 0, 0, 170 });
        DrawRectangle(x, y, (int)(width * fraction), height, fill);
        DrawRectangleLines(x, y, width, height, (Color){ 200, 200, 200, 120 });

        if (label != nullptr)
            DrawText(label, x + 6, y + (height - 10) / 2, 10, RAYWHITE);
    }

    void DrawCard(int x, int y, int index, const UpgradeDef& def, int rank, bool milestone)
    {
        const Color edge = milestone ? MILESTONE_EDGE : CARD_EDGE;

        DrawRectangle(x, y, CARD_WIDTH, CARD_HEIGHT, CARD_FILL);
        DrawRectangleLines(x, y, CARD_WIDTH, CARD_HEIGHT, edge);

        DrawRectangle(x, y, BADGE_SIZE, BADGE_SIZE, edge);
        DrawText(TextFormat("%i", index + 1), x + 7, y + 3, TITLE_SIZE, BLACK);

        DrawText(def.Name, x + BADGE_SIZE + 6, y + 4, TITLE_SIZE, RAYWHITE);

        // The numbers, which are the whole point of the card.
        char lines[3][64] = { { 0 } };
        const int lineCount = WrapText(def.Description, BODY_SIZE, CARD_WIDTH - CARD_PADDING * 2, lines, 3);
        for (int i = 0; i < lineCount; i++)
            DrawText(lines[i], x + CARD_PADDING, y + BADGE_SIZE + 8 + i * (BODY_SIZE + 3), BODY_SIZE, LIGHTGRAY);

        const char* footer = (def.MaxRank > 1) ? TextFormat("%i/%i", rank, (int)def.MaxRank)
                                               : (milestone ? "PATH" : "ONCE");
        DrawText(footer, x + CARD_PADDING, y + CARD_HEIGHT - BODY_SIZE - 6, BODY_SIZE,
                 milestone ? MILESTONE_EDGE : GRAY);
    }
}

void UpgradeUI::Update(NetClient& net)
{
    const UpgradeState& upgrades = net.GetUpgrades();

    // Nothing to press until the server has both owed us a pick and told us what
    // the pick is between.
    if (upgrades.PendingPicks() <= 0 || upgrades.OfferCount() <= 0)
        return;

    // IsKeyPressed rather than IsKeyDown: holding a key would otherwise spend
    // every banked pick on whatever happened to be in that slot.
    const int keys[UPGRADE_OFFER_COUNT] = { KEY_ONE, KEY_TWO, KEY_THREE };

    for (int i = 0; i < upgrades.OfferCount() && i < UPGRADE_OFFER_COUNT; i++)
    {
        if (!IsKeyPressed(keys[i]))
            continue;

        // The server decides whether this is allowed and answers with the new
        // state. Nothing is applied locally on the strength of a keypress.
        net.SendUpgradeChoice(upgrades.OfferedId(i));
        return;
    }
}

void UpgradeUI::Draw(NetClient& net)
{
    const UpgradeState& upgrades = net.GetUpgrades();

    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();

    // --- health and progress, centred on the bottom edge ---

    const int barX = (screenWidth - BAR_WIDTH) / 2;
    const int levelBarY = screenHeight - BAR_BOTTOM_MARGIN - BAR_HEIGHT;
    const int healthBarY = levelBarY - BAR_GAP - BAR_HEIGHT;

    const float maxHealth = net.GetMaxHealth();
    const float health = net.GetHealth();
    const float healthFraction = (maxHealth > 0.0f) ? health / maxHealth : 0.0f;

    // Red once there is little enough left that it is worth noticing.
    const Color healthFill = (healthFraction > 0.5f) ? (Color){ 70, 190, 110, 230 }
                           : (healthFraction > 0.25f) ? (Color){ 220, 180, 60, 230 }
                                                      : (Color){ 210, 70, 70, 230 };

    DrawBar(barX, healthBarY, BAR_WIDTH, BAR_HEIGHT, healthFraction, healthFill,
            TextFormat("HEALTH  %i / %i", (int)(health + 0.5f), (int)(maxHealth + 0.5f)));

    const int level = upgrades.Level();
    const int levelFloor = XpToReachLevel(level);
    const int levelCeiling = XpToReachLevel(level + 1);
    const int span = levelCeiling - levelFloor;
    const float levelFraction = (level >= MAX_LEVEL || span <= 0)
                              ? 1.0f
                              : (float)(upgrades.Xp() - levelFloor) / (float)span;

    DrawBar(barX, levelBarY, BAR_WIDTH, BAR_HEIGHT, levelFraction,
            (Color){ 90, 140, 200, 230 },
            (level >= MAX_LEVEL) ? TextFormat("LEVEL %i  (max)", level)
                                 : TextFormat("LEVEL %i", level));

    // --- the cards ---

    const int pending = upgrades.PendingPicks();
    const int offerCount = upgrades.OfferCount();
    if (pending <= 0 || offerCount <= 0)
        return;

    // A milestone offer is the branch choice, and is worth marking out from the
    // ordinary run of stat cards.
    bool milestone = false;
    for (int i = 0; i < offerCount; i++)
    {
        const UpgradeDef* def = UpgradeCatalog::Find(upgrades.OfferedId(i));
        if (def != nullptr && def->MilestoneLevel != 0)
            milestone = true;
    }

    const int totalWidth = offerCount * CARD_WIDTH + (offerCount - 1) * CARD_GAP;
    const int startX = (screenWidth - totalWidth) / 2;

    // Stacked directly above the health bar, with room for the heading over it.
    const int cardY = healthBarY - 14 - CARD_HEIGHT;

    const char* heading = milestone ? "CHOOSE YOUR PATH" : "CHOOSE AN UPGRADE";
    const int headingWidth = MeasureText(heading, 16);
    DrawText(heading, (screenWidth - headingWidth) / 2, cardY - 20, 16,
             milestone ? MILESTONE_EDGE : RAYWHITE);

    // Banked picks are worth saying out loud, because the next set appears the
    // instant this one is spent and that would otherwise look like a glitch.
    if (pending > 1)
    {
        const char* queued = TextFormat("+%i more", pending - 1);
        const int queuedWidth = MeasureText(queued, 10);
        DrawText(queued, (screenWidth - queuedWidth) / 2, cardY - 34, 10, GRAY);
    }

    for (int i = 0; i < offerCount; i++)
    {
        const UpgradeDef* def = UpgradeCatalog::Find(upgrades.OfferedId(i));
        if (def == nullptr)
            continue;

        DrawCard(startX + i * (CARD_WIDTH + CARD_GAP), cardY, i, *def,
                 upgrades.RankOf(def->Id), def->MilestoneLevel != 0);
    }
}
