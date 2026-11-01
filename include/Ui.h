#pragma once
#include "GameCore.h"
#include "WledClient.h"
#include <TFT_eSPI.h>

struct UiState {
    const gd::Scoreboard *scoreboard = nullptr;
    const WledClient *leds = nullptr;
    uint32_t selectedGameId = 0;
    int selectedLed = -1;
    int gamePage = 0;
    bool wifiConnected = false;
    bool nhlConnected = false;
    String ip;
    String nhlError;
    String message;
};

enum class Page : uint8_t { Scores, Lights, Setup };
enum class ActionType : uint8_t {
    None, SelectGame, SelectLed, PreviousGames, NextGames,
    ScanLeds, TeamAway, TeamHome, PowerOff, RefreshNhl
};
struct UiAction {
    ActionType type;
    int index;
    UiAction(ActionType type_ = ActionType::None, int index_ = -1)
        : type(type_), index(index_) {}
};

class Ui {
public:
    void begin();
    void draw(const UiState &state);
    UiAction touch(int16_t x, int16_t y, const UiState &state);
    Page page() const { return page_; }
    void setPage(Page p) { page_ = p; }
private:
    TFT_eSPI tft_;
    Page page_ = Page::Scores;
    void header(const UiState &state, const char *title);
    void footer();
    void scores(const UiState &state);
    void lights(const UiState &state);
    void setup(const UiState &state);
    void button(int x, int y, int w, int h, const String &label, uint16_t color);
};
