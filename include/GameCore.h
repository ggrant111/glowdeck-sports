#pragma once
#include <stdint.h>
#include <stddef.h>

namespace gd {
constexpr size_t MAX_GAMES = 16;
constexpr size_t MAX_LEDS = 8;

struct Game {
    uint32_t id = 0;
    char away[5] = "";
    char home[5] = "";
    char state[12] = "";
    char clock[14] = "";
    uint8_t awayScore = 0;
    uint8_t homeScore = 0;
    uint8_t period = 0;
};

struct Scoreboard {
    Game games[MAX_GAMES];
    uint8_t count = 0;
    char date[11] = "";
    bool valid = false;
};

struct GoalChange {
    bool changed = false;
    uint8_t awayAdded = 0;
    uint8_t homeAdded = 0;
};

// Score-change detector is display-only for Sprint 1. NEVER fires WLED commands.
GoalChange detectGoalChange(const Game &before, const Game &after);
const Game* findGame(const Scoreboard &scores, uint32_t gameId);
bool isLive(const Game &game);
uint32_t teamColor(const char *abbrev);
const char* gameStateText(const Game &game);
}
