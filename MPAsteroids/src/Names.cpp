#include "include/Names.h"

#include "include/raylib/raylib.h"
#include "include/networking/NetConstants.h"

#include <cstdio>
#include <vector>

namespace Names
{
    namespace
    {
        const char* const NAMES_FILE = "resources/names.txt";

        std::vector<std::string> adjectives;
        std::vector<std::string> nouns;

        // Used only when the file is missing or unreadable, so a bad asset build
        // still gets a usable name instead of an empty one.
        const char* const kFallbackAdjectives[] = { "Swift", "Rogue", "Lunar", "Solar" };
        const char* const kFallbackNouns[] = { "Pilot", "Hawk", "Drift", "Racer" };

        // Also strips the carriage return left on each line by files saved with
        // Windows line endings, which would otherwise end up inside a name.
        void Trim(std::string& text)
        {
            size_t start = 0;
            while (start < text.size() && (text[start] == ' ' || text[start] == '\t'))
                start++;

            size_t end = text.size();
            while (end > start)
            {
                char last = text[end - 1];
                if (last != ' ' && last != '\t' && last != '\r')
                    break;
                end--;
            }

            text = text.substr(start, end - start);
        }

        // How many characters the words may take up once the two digits and the
        // terminator have their share.
        const size_t ROOM_FOR_WORDS = MAX_PLAYER_NAME_LENGTH - 1 - 2;

        // Trimming is silent otherwise, so someone adding a long word would only
        // notice when a clipped name turned up on the leaderboard.
        void WarnIfNamesWouldBeCut()
        {
            size_t longestAdjective = 0;
            size_t longestNoun = 0;
            for (const std::string& word : adjectives)
                longestAdjective = word.size() > longestAdjective ? word.size() : longestAdjective;
            for (const std::string& word : nouns)
                longestNoun = word.size() > longestNoun ? word.size() : longestNoun;

            if (longestAdjective + longestNoun > ROOM_FOR_WORDS)
            {
                printf("[NAMES] longest pairing is %zu characters but only %zu fit, so some names will be cut short\n",
                       longestAdjective + longestNoun, ROOM_FOR_WORDS);
            }
        }

        void UseFallbackNames()
        {
            printf("[NAMES] %s missing or empty, using the built-in list\n", NAMES_FILE);

            adjectives.clear();
            nouns.clear();
            for (const char* word : kFallbackAdjectives) adjectives.push_back(word);
            for (const char* word : kFallbackNouns) nouns.push_back(word);
        }
    }

    void Init()
    {
        adjectives.clear();
        nouns.clear();

        char* contents = LoadFileText(NAMES_FILE);
        if (contents == nullptr)
        {
            UseFallbackNames();
            return;
        }

        // Words go into whichever list the most recent [section] header named, so
        // anything before the first header is ignored rather than guessed at.
        std::vector<std::string>* section = nullptr;
        std::string line;

        for (const char* c = contents; ; ++c)
        {
            if (*c != '\0' && *c != '\n')
            {
                line += *c;
                continue;
            }

            Trim(line);
            if (!line.empty() && line[0] != '#')
            {
                if (line == "[adjectives]") section = &adjectives;
                else if (line == "[nouns]") section = &nouns;
                else if (section != nullptr) section->push_back(line);
            }
            line.clear();

            if (*c == '\0') break;
        }

        UnloadFileText(contents);

        if (adjectives.empty() || nouns.empty())
        {
            UseFallbackNames();
            return;
        }

        WarnIfNamesWouldBeCut();
    }

    std::string MakeRandom()
    {
        if (adjectives.empty() || nouns.empty())
            Init();

        const std::string& adjective = adjectives[GetRandomValue(0, (int)adjectives.size() - 1)];
        const std::string& noun = nouns[GetRandomValue(0, (int)nouns.size() - 1)];

        // The digits are what separate two players who drew the same pair, so the
        // words are trimmed to make room for them rather than the other way round.
        std::string words = adjective + noun;
        if (words.size() > ROOM_FOR_WORDS)
            words.resize(ROOM_FOR_WORDS);

        char buffer[MAX_PLAYER_NAME_LENGTH];
        std::snprintf(buffer, sizeof(buffer), "%s%02d", words.c_str(), GetRandomValue(0, 99));
        return std::string(buffer);
    }
}
