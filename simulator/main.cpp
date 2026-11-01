// Offline desktop test of portable game state and event semantics; no network, screen, or WLED required.
#include "GameCore.h"
#include <iostream>
#include <cstring>
#include <cstdlib>

static gd::Game game(unsigned id, const char* state, unsigned away, unsigned home) {
    gd::Game result;
    result.id=id;
    std::strncpy(result.away,"UTA",sizeof(result.away)-1);
    std::strncpy(result.home,"BOS",sizeof(result.home)-1);
    std::strncpy(result.state,state,sizeof(result.state)-1);
    result.awayScore=away;
    result.homeScore=home;
    return result;
}
int main() {
    gd::Game frames[]={game(2026020001,"FUT",0,0),game(2026020001,"LIVE",0,0),
        game(2026020001,"LIVE",1,0),game(2026020001,"LIVE",1,1),game(2026020001,"OFF",2,3)};
    unsigned displayAlerts=0;
    std::cout << "GlowDeck GameDay offline simulator — UTA @ BOS\n";
    for (size_t i=0; i<sizeof(frames)/sizeof(frames[0]); ++i) {
        const auto &current=frames[i];
        std::cout << "Frame " << i << ": " << unsigned(current.awayScore) << "-" << unsigned(current.homeScore)
                  << " [" << gd::gameStateText(current) << "]";
        if (i) {
            auto delta=gd::detectGoalChange(frames[i-1],current);
            if (delta.changed) { ++displayAlerts; std::cout << "  DISPLAY-only alert"; }
        }
        std::cout << '\n';
    }
    std::cout << "Display alerts: " << displayAlerts << " | WLED auto actions: 0 (disabled)\n";
    return displayAlerts == 2 ? EXIT_SUCCESS : EXIT_FAILURE;
}
