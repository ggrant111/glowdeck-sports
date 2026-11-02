#pragma once
#include "GameCore.h"
#include "AudioModel.h"
#include "WledClient.h"
#include "FollowManager.h"
#include <TFT_eSPI.h>

struct UiState {
    const FollowManager *following=nullptr;
    const gd::Scoreboard *scoreboard = nullptr;
    const WledClient *leds = nullptr;
    uint32_t selectedGameId = 0;
    int selectedLed = -1;
    int gamePage = 0;
    int ledPage = 0;
    const gd::Game *tracked=nullptr;
    uint8_t scheduleDay=0;
    bool hour24=true;
    bool resumeEnabled=false;
    bool waiting = false;
    bool wifiConnected = false;
    bool nhlConnected = false;
    bool goalArmed = false;
    bool goalActive = false;
    bool goalQueued = false;
    gd::CelebrationMode celebrationMode = gd::CelebrationMode::Away;
    bool goalPaused = false;
    bool goalBaselineReady = false;
    uint8_t goalDelay = 0;
    bool restorePending = false;
    bool ddp=false;uint8_t effectSelection=0,effectMask=127;
    bool soundEnabled = false;
    bool soundBusy = false;
    uint8_t soundVolume = 50;
    gd::SoundKind awaySound = gd::SoundKind::Horn;
    gd::SoundKind homeSound = gd::SoundKind::Horn;
    String ip;
    String nhlError;
    String message;
};

enum class Page : uint8_t { Scores, GameDay, Lights, Setup, Audio, Effects, Following };
enum class ActionType : uint8_t {
    FollowAdd, FollowStart, FollowStop, PreviousLeds, NextLeds, ScheduleDay, ToggleResume,
    None, SelectGame, SelectLed, PreviousGames, NextGames, ScanLeds,
    TeamAway, TeamHome, PowerOff, RefreshNhl, ToggleArm,
    CelebrateAway, CelebrateHome, CelebrateBoth, DelayMinus, DelayPlus,
    AudioToggle, AudioVolumeDown, AudioVolumeUp, AudioAwayNext, AudioHomeNext,
    EffectsToggle, EffectsNext, EffectsEnable, EffectsAway, EffectsHome,
    AudioPreviewAway, AudioPreviewHome
};
struct UiAction {
    ActionType type;
    int index;
    UiAction(ActionType t = ActionType::None, int i = -1) : type(t), index(i) {}
};
class Ui {
public:
    void begin();
    void calibrationTarget(int index);
    void draw(const UiState &state);
    UiAction touch(int16_t x, int16_t y, const UiState &state);
    Page page() const { return page_; }
    void setPage(Page p) { page_ = p; }
private:
    TFT_eSPI tft_;
    String lastRenderKey_;
    bool rendered_ = false;
    String renderKey(const UiState &state) const;
    Page page_ = Page::Scores;
    void header(const UiState &state, const char *title);
    void footer();
    void scores(const UiState &state);
    void gameday(const UiState &state);
    void lights(const UiState &state);
    void setup(const UiState &state);
    void audio(const UiState &state);
    void effects(const UiState &state);
    void button(int x, int y, int w, int h, const String &label, uint16_t color);
};
