#pragma once
#include "GameCore.h"
#include <Arduino.h>
class NhlClient {
public:
    // Replaces output only on a valid response; caller retains last known score on errors.
    bool poll(gd::Scoreboard &output, String &error);
};
