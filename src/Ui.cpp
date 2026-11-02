#include "Ui.h"

namespace {
const uint16_t BG = 0x0842, CARD = 0x18E3, FG = 0xFFFF;
const uint16_t MUTED = 0xAD75, TEAL = 0x3410, GOLD = 0xFE00, SELECT = 0x29E6;
constexpr int FOOTER_Y = 206;
}

void Ui::begin() {
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
    tft_.init();
    tft_.setRotation(1);
    tft_.fillScreen(BG);
    tft_.setTextWrap(false);
}
void Ui::button(int x, int y, int w, int h, const String &label, uint16_t color) {
    tft_.fillRoundRect(x, y, w, h, 7, color);
    tft_.setTextColor(FG, color);
    tft_.setTextDatum(MC_DATUM);
    tft_.drawString(label, x+w/2, y+h/2, 2);
    tft_.setTextDatum(TL_DATUM);
}
void Ui::header(const UiState &state, const char *title) {
    tft_.fillRect(0, 0, 320, 34, CARD);
    tft_.setTextColor(FG, CARD);
    tft_.drawString("GLOWDECK", 10, 9, 2);
    tft_.setTextColor(TEAL, CARD);
    tft_.drawString(title, 110, 9, 2);
    tft_.setTextColor(state.wifiConnected ? TFT_GREEN : TFT_RED, CARD);
    tft_.drawString(state.wifiConnected ? "WiFi" : "Offline", 264, 9, 2);
}
void Ui::footer() {
    tft_.fillRect(0, FOOTER_Y-1, 320, 35, CARD);
    button(2, 208, 77, 28, "SCORES", page_ == Page::Scores ? TEAL : CARD);
    button(82, 208, 77, 28, "GAME", page_ == Page::GameDay ? TEAL : CARD);
    button(162, 208, 77, 28, "LIGHTS", page_ == Page::Lights ? TEAL : CARD);
    button(242, 208, 76, 28, "SETUP", page_ == Page::Setup ? TEAL : CARD);
}
void Ui::scores(const UiState &state) {
    const gd::Scoreboard &s = *state.scoreboard;
    if (!s.valid) {
        tft_.setTextColor(MUTED, BG);
        tft_.drawCentreString("Waiting for NHL scores...", 160, 75, 2);
        tft_.drawCentreString("Connect WiFi to load games", 160, 110, 2);
        return;
    }
    if (s.count == 0) {
        tft_.setTextColor(MUTED, BG);
        tft_.drawCentreString("No NHL games scheduled", 160, 76, 2);
        tft_.drawCentreString(s.date, 160, 109, 2);
        return;
    }
    tft_.setTextColor(MUTED, BG);
    tft_.drawString(s.date, 7, 36, 2);
    tft_.drawRightString("<      >", 311, 36, 2);
    for (int row=0; row<4; ++row) {
        int index = state.gamePage * 4 + row;
        if (index >= s.count) break;
        const gd::Game &g = s.games[index];
        const int y = 58 + row*36;
        bool selected = state.selectedGameId == g.id;
        tft_.fillRoundRect(6, y, 308, 32, 5, selected ? SELECT : CARD);
        tft_.setTextColor(FG, selected ? SELECT : CARD);
        String matchup = String(g.away) + " @ " + g.home;
        tft_.drawString(matchup, 14, y+8, 2);
        char score[14];
        snprintf(score, sizeof(score), "%u - %u", g.awayScore, g.homeScore);
        tft_.drawCentreString(score, 196, y+8, 2);
        tft_.setTextColor(gd::isLive(g) ? GOLD : MUTED, selected ? SELECT : CARD);
        tft_.drawRightString(gd::gameStateText(g), 303, y+8, 2);
    }
}
void Ui::gameday(const UiState &state) {
    const gd::Game *game = gd::findGame(*state.scoreboard, state.selectedGameId);
    if (!game) {
        tft_.setTextColor(MUTED, BG);
        tft_.drawCentreString("Choose a game in SCORES", 160, 66, 2);
        tft_.drawCentreString("Then choose a WLED in LIGHTS", 160, 102, 2);
        return;
    }
    tft_.setTextColor(FG, BG);
    tft_.drawCentreString(String(game->away) + "  " + String(game->awayScore) + " : " +
             String(game->homeScore) + "  " + game->home, 160, 42, 4);
    tft_.setTextColor(MUTED, BG);
    tft_.drawCentreString(String(gd::gameStateText(*game)), 160, 73, 2);
    tft_.drawString("CELEBRATE", 10, 87, 2);
    button(8, 105, 96, 29, game->away,
        state.celebrationMode == gd::CelebrationMode::Away ? TEAL : CARD);
    button(111, 105, 96, 29, game->home,
        state.celebrationMode == gd::CelebrationMode::Home ? TEAL : CARD);
    button(214, 105, 98, 29, "BOTH",
        state.celebrationMode == gd::CelebrationMode::Both ? TEAL : CARD);
    tft_.setTextColor(FG, BG);
    tft_.drawString("TV DELAY", 9, 143, 2);
    button(115, 138, 38, 27, "-", CARD);
    tft_.drawCentreString(String(state.goalDelay) + "s", 181, 143, 2);
    button(238, 138, 39, 27, "+", CARD);
    button(12, 166, 296, 33, state.restorePending ? "RESTORING LIGHTS" :
        (state.goalActive ? "GOAL LIGHTS ACTIVE" : (state.goalArmed ? "DISARM GOALS" : "ARM GOAL LIGHTS")),
        state.goalArmed ? TEAL : CARD);
    if (state.goalQueued || (state.goalArmed && state.goalPaused)) {
        tft_.setTextColor(GOLD, BG);
        tft_.drawCentreString(state.goalQueued ? "Goal waiting for TV delay" :
             "Waiting for NHL PBP baseline", 160, 199, 1);
    }
}
void Ui::lights(const UiState &state) {
    tft_.setTextColor(MUTED, BG);
    if (state.leds->size() == 0) {
        tft_.drawCentreString("No WLED devices found", 160, 64, 2);
        tft_.drawCentreString("Tap SCAN or use web setup", 160, 99, 2);
    }
    for (size_t i=0; i<state.leds->size() && i<3; ++i) {
        const LedDevice &d = state.leds->at(i);
        const int y = 37 + (int)i*39;
        bool sel = (int)i == state.selectedLed;
        tft_.fillRoundRect(7, y, 305, 35, 6, sel ? SELECT : CARD);
        tft_.setTextColor(FG, sel ? SELECT : CARD);
        tft_.drawString(d.name.substring(0,16), 14, y+3, 2);
        tft_.setTextColor(d.online ? TFT_GREEN : MUTED, sel ? SELECT : CARD);
        tft_.drawString(d.ip, 14, y+19, 1);
    }
    button(4, 165, 75, 33, "AWAY", TEAL);
    button(83, 165, 75, 33, "HOME", TEAL);
    button(162, 165, 75, 33, "OFF", CARD);
    button(241, 165, 75, 33, "SCAN", CARD);
}
void Ui::setup(const UiState &state) {
    tft_.setTextColor(FG, BG);
    tft_.drawString("Wi-Fi", 9, 41, 2);
    tft_.setTextColor(MUTED, BG);
    tft_.drawString(state.wifiConnected ? state.ip : "Not connected", 92, 41, 2);
    tft_.setTextColor(FG, BG);
    tft_.drawString("NHL API", 9, 72, 2);
    tft_.setTextColor(state.nhlConnected ? TFT_GREEN : GOLD, BG);
    tft_.drawString(state.nhlConnected ? "Connected" : "Awaiting data", 92, 72, 2);
    tft_.setTextColor(MUTED, BG);
    if (!state.nhlError.isEmpty()) tft_.drawString(state.nhlError.substring(0,36), 8, 98, 1);
    tft_.drawString("Configure WLED devices:", 9, 123, 2);
    tft_.drawString(state.wifiConnected ? "http://" + state.ip : "Connect to Wi-Fi first", 9, 143, 2);
    button(13, 169, 294, 29, "REFRESH SCORES", TEAL);
}
void Ui::draw(const UiState &state) {
    tft_.fillScreen(BG);
    header(state, page_ == Page::Scores ? "NHL" : (page_ == Page::GameDay ? "GAME" : (page_ == Page::Lights ? "WLED" : "STATUS")));
    switch (page_) {
        case Page::Scores: scores(state); break;
        case Page::GameDay: gameday(state); break;
        case Page::Lights: lights(state); break;
        case Page::Setup: setup(state); break;
    }
    footer();
    // Serial logs provide full error messages; short banner here for feedback.
    if (!state.message.isEmpty() && page_ != Page::Setup) {
        tft_.fillRect(0, 195, 320, 10, BG);
        tft_.setTextColor(GOLD, BG);
        tft_.drawCentreString(state.message.substring(0,48), 160, 195, 1);
    }
}
UiAction Ui::touch(int16_t x, int16_t y, const UiState &state) {
    if (y >= FOOTER_Y) {
        if (x < 80) page_ = Page::Scores;
        else if (x < 160) page_ = Page::GameDay;
        else if (x < 240) page_ = Page::Lights;
        else page_ = Page::Setup;
        return {};
    }
    if (page_ == Page::Scores) {
        if (y >= 35 && y < 56) {
            if (x > 270 && x < 292) return {ActionType::PreviousGames, -1};
            if (x >= 292) return {ActionType::NextGames, -1};
        }
        if (y >= 58 && y < 202) {
            int row = (y - 58) / 36;
            int idx = state.gamePage * 4 + row;
            if (row < 4 && idx < state.scoreboard->count) return {ActionType::SelectGame, idx};
        }
    } else if (page_ == Page::GameDay) {
        if (y >= 105 && y < 137) {
            if (x < 107) return {ActionType::CelebrateAway};
            if (x < 211) return {ActionType::CelebrateHome};
            return {ActionType::CelebrateBoth};
        }
        if (y >= 138 && y < 166) {
            if (x >= 112 && x < 157) return {ActionType::DelayMinus};
            if (x >= 235 && x < 280) return {ActionType::DelayPlus};
        }
        if (y >= 166 && y < 200) return {ActionType::ToggleArm};
    } else if (page_ == Page::Lights) {
        if (y >= 37 && y < 154) {
            int row = (y-37)/39;
            if (row < (int)state.leds->size() && row < 3) return {ActionType::SelectLed, row};
        }
        if (y >= 165 && y < 199) {
            if (x < 80) return {ActionType::TeamAway, -1};
            if (x < 161) return {ActionType::TeamHome, -1};
            if (x < 240) return {ActionType::PowerOff, -1};
            return {ActionType::ScanLeds, -1};
        }
    } else if (page_ == Page::Setup && y >= 169 && y < 200) {
        return {ActionType::RefreshNhl, -1};
    }
    return {};
}
