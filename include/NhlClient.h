#pragma once
#include "GameCore.h"
#include <Arduino.h>
class NhlClient {
public:
    bool poll(gd::Scoreboard &output, String &error, uint8_t dayOffset=0);
    // Server's eventId + eventOwnerTeamId, not inferred scoreboard deltas.
    bool pollGoals(uint32_t gameId, gd::GoalFeed &out, String &error, gd::Game *details=nullptr);
};
