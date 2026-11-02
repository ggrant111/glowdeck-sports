#pragma once
#include <stdint.h>
#include <stddef.h>
#include "EventEffects.h"

namespace gd {
constexpr size_t MAX_GAMES = 16;
constexpr size_t MAX_LEDS = 8;
constexpr size_t MAX_GOALS_PER_GAME = 32;
enum class CelebrationMode : uint8_t { Away=0, Home=1, Both=2 };
inline CelebrationMode validatedMode(uint8_t value) {
    return value == 1 ? CelebrationMode::Home : value == 2 ? CelebrationMode::Both : CelebrationMode::Away;
}
inline const char* celebrationName(CelebrationMode mode) {
    return mode == CelebrationMode::Both ? "BOTH" : (mode == CelebrationMode::Home ? "HOME" : "AWAY");
}
// Compact NHL /play-by-play sample: latest event ID includes NON-goals, so
// the goal engine can deduplicate without retaining the complete PBP log.
struct GoalPlay { uint32_t id = 0, ownerId = 0; };
struct ExtraPlay { uint32_t id=0, ownerId=0; SportEvent kind=SportEvent::PeriodStart; };
struct GoalFeed {
    uint32_t gameId = 0, homeId = 0, awayId = 0, latestEventId = 0;
    GoalPlay goals[MAX_GOALS_PER_GAME];
    uint8_t count = 0;
    ExtraPlay extra[64];
    uint8_t extraCount=0;
    char state[12] = "";
    bool final=false;
    bool valid = false;
};


struct Game {
    uint32_t id = 0;
    char away[5] = "";
    char home[5] = "";
    char state[12] = "";
    char periodType[5] = "";
    char clock[14] = "";
    char startTime[25] = "";
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
bool isUpcoming(const Game &game);
uint32_t teamColor(const char *abbrev);
const char* gameStateText(const Game &game);
}
