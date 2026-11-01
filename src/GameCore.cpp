#include "GameCore.h"
#include <string.h>

namespace gd {
GoalChange detectGoalChange(const Game &before, const Game &after) {
    GoalChange out;
    if (!before.id || before.id != after.id || !isLive(after)) return out;
    // Decreasing scores signal corrections or a resumed earlier frame; not a goal.
    if (after.awayScore < before.awayScore || after.homeScore < before.homeScore) return out;
    out.awayAdded = after.awayScore - before.awayScore;
    out.homeAdded = after.homeScore - before.homeScore;
    out.changed = out.awayAdded || out.homeAdded;
    return out;
}
const Game* findGame(const Scoreboard &scores, uint32_t gameId) {
    if (!gameId) return nullptr;
    for (size_t i=0; i<scores.count; ++i) if (scores.games[i].id == gameId) return &scores.games[i];
    return nullptr;
}
bool isLive(const Game &g) { return !strcmp(g.state, "LIVE") || !strcmp(g.state, "CRIT"); }
const char* gameStateText(const Game &g) {
    if (isLive(g)) return "LIVE";
    if (!strcmp(g.state, "FINAL") || !strcmp(g.state, "OFF")) return "FINAL";
    if (!strcmp(g.state, "FUT") || !strcmp(g.state, "PRE")) return "UPCOMING";
    return g.state[0] ? g.state : "UNKNOWN";
}
struct TeamColor { const char *abbrev; uint32_t rgb; };
static const TeamColor colors[] = {
  {"ANA",0xF47A38},{"BOS",0xFFB81C},{"BUF",0x003087},{"CGY",0xC8102E},
  {"CAR",0xCC0000},{"CHI",0xCF0A2C},{"COL",0x6F263D},{"CBJ",0x002654},
  {"DAL",0x00843D},{"DET",0xCE1126},{"EDM",0xFF4C00},{"FLA",0xC8102E},
  {"LAK",0xA2AAAD},{"MIN",0x154734},{"MTL",0xAF1E2D},{"NSH",0xFFB81C},
  {"NJD",0xCE1126},{"NYI",0x00539B},{"NYR",0x0038A8},{"OTT",0xE31837},
  {"PHI",0xF74902},{"PIT",0xFCB514},{"SJS",0x006D75},{"SEA",0x99D9D9},
  {"STL",0x002F87},{"TBL",0x002868},{"TOR",0x00205B},{"UTA",0x6CACE4},
  {"VAN",0x00205B},{"VGK",0xB4975A},{"WSH",0xC8102E},{"WPG",0x041E42},
};
uint32_t teamColor(const char *abbrev) {
    if (!abbrev) return 0x31837A;
    for (const auto &t : colors) if (!strcmp(t.abbrev, abbrev)) return t.rgb;
    return 0x31837A;
}
}
