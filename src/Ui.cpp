#include "Ui.h"
#include <time.h>
namespace {
String localStart(const char *stamp) {
    struct tm utc={};
    if(!strptime(stamp,"%Y-%m-%dT%H:%M:%S",&utc))return "TBD";
    // mktime uses local TZ; compensate through timegm equivalent using UTC civil conversion.
    int year=utc.tm_year+1900,month=utc.tm_mon+1;year-=month<=2;
    const int era=(year>=0?year:year-399)/400;const unsigned yoe=year-era*400;
    const unsigned doy=(153*(month+(month>2?-3:9))+2)/5+utc.tm_mday-1;
    time_t epoch=(era*146097+int(yoe*365+yoe/4-yoe/100+doy)-719468)*86400LL+utc.tm_hour*3600+utc.tm_min*60;
    struct tm local;localtime_r(&epoch,&local);char out[10];strftime(out,sizeof(out),"%H:%M",&local);return out;
}
}


namespace {
const uint16_t BG = 0x0842, CARD = 0x18E3, FG = 0xFFFF;
const uint16_t MUTED = 0xAD75, TEAL = 0x3410, GOLD = 0xFE00, SELECT = 0x29E6;
constexpr int FOOTER_Y = 206;
}

void Ui::begin() {
    rendered_ = false;
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);
    tft_.init();
    tft_.setRotation(GD_DISPLAY_ROTATION);
    Serial.printf("Display rotation=%d size=%dx%d\n", GD_DISPLAY_ROTATION, tft_.width(), tft_.height());
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
    button(1, 208, 62, 28, "SCORES", page_ == Page::Scores ? TEAL : CARD);
    button(65, 208, 62, 28, "GAME", page_ == Page::GameDay ? TEAL : CARD);
    button(129, 208, 62, 28, "LIGHTS", page_ == Page::Lights ? TEAL : CARD);
    button(193, 208, 62, 28, "SETUP", page_ == Page::Setup ? TEAL : CARD);
    button(257, 208, 62, 28, "SOUND", page_ == Page::Audio ? TEAL : CARD);
}
void Ui::scores(const UiState &state) {
    const gd::Scoreboard &s = *state.scoreboard;
    if (!s.valid) {
        button(5,35,180,21,"NEXT DAY >",CARD);
        tft_.setTextColor(MUTED, BG);
        tft_.drawCentreString("Waiting for NHL scores...", 160, 75, 2);
        tft_.drawCentreString("Connect WiFi to load games", 160, 110, 2);
        return;
    }
    if (s.count == 0) {
        button(5,35,180,21,"NEXT DAY >",CARD);
        tft_.setTextColor(MUTED, BG);
        tft_.drawCentreString("No NHL games scheduled", 160, 76, 2);
        tft_.drawCentreString(s.date, 160, 109, 2);
        return;
    }
    tft_.setTextColor(MUTED, BG);
    button(5,35,180,21,state.scheduleDay==0?"TODAY >":state.scheduleDay==1?"TOMORROW >":String(s.date)+" >",CARD);
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
        tft_.drawCentreString(gd::isUpcoming(g)?localStart(g.startTime):String(score),196,y+8,2);
        tft_.setTextColor(gd::isLive(g) ? GOLD : MUTED, selected ? SELECT : CARD);
        tft_.drawRightString(gd::gameStateText(g), 303, y+8, 2);
    }
}
void Ui::gameday(const UiState &state) {
    const gd::Game *game = state.tracked;
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
    tft_.drawCentreString(String(gd::gameStateText(*game))+(gd::isUpcoming(*game)?" "+localStart(game->startTime):String("")), 160, 73, 2);
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
        (state.goalActive ? "EVENT LIGHTS ACTIVE" : (state.goalArmed ? "DISARM GAMEDAY" : "ARM GAMEDAY")),
        state.goalArmed ? TEAL : CARD);
    if (state.waiting || state.goalQueued || (state.goalArmed && state.goalPaused)) {
        tft_.setTextColor(GOLD, BG);
        tft_.drawCentreString(state.waiting ? "ARMED - waiting for game start" : state.goalQueued ? "Goal waiting for TV delay" :
             "Waiting for NHL PBP baseline", 160, 199, 1);
    }
}
void Ui::lights(const UiState &state) {
    tft_.setTextColor(MUTED, BG);
    if (state.leds->size() == 0) {
        tft_.drawCentreString("No WLED devices found", 160, 64, 2);
        tft_.drawCentreString("Tap SCAN or use web setup", 160, 99, 2);
    }
    tft_.setTextColor(MUTED,BG);
    tft_.drawCentreString(String(state.leds->size())+" devices / "+String(state.ledPage+1)+" of "+String(max(1,(int(state.leds->size())+2)/3)),160,36,1);
    button(6,35,42,20,"<",CARD); button(272,35,42,20,">",CARD);
    for (size_t i=state.ledPage*3; i<state.leds->size() && i<size_t(state.ledPage*3+3); ++i) {
        const LedDevice &d = state.leds->at(i);
        const int y = 57 + (int(i)%3)*34;
        bool sel = (int)i == state.selectedLed;
        tft_.fillRoundRect(7, y, 305, 31, 6, sel ? SELECT : CARD);
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
    button(8,116,304,24,state.resumeEnabled?"RESUME AFTER REBOOT: ON":"RESUME AFTER REBOOT: OFF",CARD);
    tft_.drawString(state.wifiConnected ? "http://" + state.ip : "Connect to Wi-Fi first", 9, 143, 2);
    button(8, 169, 147, 29, "REFRESH", TEAL);
    button(163,169,149,29,"EFFECTS",TEAL);
}
namespace {
String soundName(gd::SoundKind kind) {
    switch(kind) {
        case gd::SoundKind::Horn: return "HORN";
        case gd::SoundKind::DoubleHorn: return "DOUBLE";
        case gd::SoundKind::Siren: return "SIREN";
        case gd::SoundKind::CustomWav: return "CUSTOM";
        default: return "OFF";
    }
}
}
void Ui::audio(const UiState &state) {
    tft_.setTextColor(FG, BG);
    tft_.drawString("Audio output", 12, 41, 2);
    button(186, 38, 121, 27, state.soundEnabled?"SOUND ON":"SOUND OFF",
        state.soundEnabled?TEAL:CARD);
    tft_.drawString("Volume", 11, 78, 2);
    button(110, 71, 40, 28, "-", CARD);
    tft_.drawCentreString(String(state.soundVolume)+"%", 195, 78, 2);
    button(266, 71, 42, 28, "+", CARD);
    tft_.drawString("AWAY", 11, 115, 2);
    button(95, 107, 212, 28, soundName(state.awaySound)+"  >", CARD);
    tft_.drawString("HOME", 11, 151, 2);
    button(95, 143, 212, 28, soundName(state.homeSound)+"  >", CARD);
    button(6, 175, 150, 28, "TRY AWAY", TEAL);
    button(163, 175, 151, 28, "TRY HOME", TEAL);
    if(state.soundBusy) {
        tft_.setTextColor(GOLD,BG);
        tft_.drawRightString("PLAYING",305,35,1);
    }
}
void Ui::effects(const UiState &s){
    button(8,38,304,28,s.ddp?"DDP ON - WHOLE STRIP":"DDP OFF - SOLID MODE",s.ddp?TEAL:CARD);
    button(8,73,304,28,String(gd::eventName(gd::SportEvent(s.effectSelection)))+" >",CARD);
    button(8,108,304,28,(s.effectMask&(1U<<s.effectSelection))?"EVENT ENABLED":"EVENT DISABLED",TEAL);
    tft_.setTextColor(MUTED,BG);tft_.drawCentreString("1-600 LEDs / 20fps / 8 seconds",160,145,2);
    button(8,169,147,28,"TRY AWAY",TEAL);button(163,169,149,28,"TRY HOME",TEAL);
}
// Compare values, not the addresses of the mutable scoreboard/WLED objects.
// Length prefixes keep arbitrary device names and messages unambiguous.
String Ui::renderKey(const UiState &s) const {
    String key;
    key.reserve(2048);
    auto add = [&key](const String &value) {
        key += String(value.length()); key += ':'; key += value;
    };
    add(String(static_cast<int>(page_)));
    add(String(s.scheduleDay));add(String(s.resumeEnabled));
    if(s.tracked){add(String(s.tracked->id));add(s.tracked->state);add(s.tracked->startTime);add(String(s.tracked->awayScore));add(String(s.tracked->homeScore));add(s.tracked->clock);}
    add(String(s.ledPage)); add(String(s.waiting));
    add(String(s.selectedGameId)); add(String(s.selectedLed)); add(String(s.gamePage));
    add(String(s.wifiConnected)); add(String(s.nhlConnected));
    add(String(s.goalArmed)); add(String(s.goalActive)); add(String(s.goalQueued));
    add(String(static_cast<int>(s.celebrationMode)));
    add(String(s.goalPaused)); add(String(s.goalBaselineReady)); add(String(s.goalDelay));
    add(String(s.restorePending)); add(String(s.ddp));
    add(String(s.effectSelection)); add(String(s.effectMask));
    add(String(s.soundEnabled)); add(String(s.soundBusy)); add(String(s.soundVolume));
    add(String(static_cast<int>(s.awaySound))); add(String(static_cast<int>(s.homeSound)));
    add(s.ip); add(s.nhlError); add(s.message);
    add(String(s.scoreboard != nullptr));
    if (s.scoreboard) {
        const auto &scores = *s.scoreboard;
        add(String(scores.valid)); add(scores.date); add(String(scores.count));
        for (size_t i=0; i<scores.count && i<gd::MAX_GAMES; ++i) {
            const auto &g=scores.games[i];
            add(String(g.id)); add(g.away); add(g.home); add(g.state); add(g.clock);add(g.startTime);
            add(String(g.awayScore)); add(String(g.homeScore)); add(String(g.period));
        }
    }
    add(String(s.leds != nullptr));
    if (s.leds) {
        add(String(s.leds->size()));
        for (size_t i=0;i<s.leds->size();++i) {
            const auto &led=s.leds->at(i);
            add(led.ip); add(led.name); add(String(led.online));
        }
    }
    return key;
}
void Ui::draw(const UiState &state) {
    const String key = renderKey(state);
    if (rendered_ && key == lastRenderKey_) return;
    lastRenderKey_ = key;
    rendered_ = true;
    tft_.fillScreen(BG);
    header(state, page_ == Page::Scores ? "NHL" : (page_ == Page::GameDay ? "GAME" : (page_ == Page::Lights ? "WLED" : (page_ == Page::Audio ? "AUDIO" : "STATUS"))));
    switch (page_) {
        case Page::Scores: scores(state); break;
        case Page::GameDay: gameday(state); break;
        case Page::Lights: lights(state); break;
        case Page::Setup: setup(state); break;
        case Page::Audio: audio(state); break;
        case Page::Effects: effects(state);break;
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
        if (x < 64) page_ = Page::Scores;
        else if (x < 128) page_ = Page::GameDay;
        else if (x < 192) page_ = Page::Lights;
        else if (x < 256) page_ = Page::Setup;
        else page_ = Page::Audio;
        return {};
    }
    if (page_ == Page::Scores) {
        if(y>=35&&y<57&&x<190)return {ActionType::ScheduleDay};
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
        if (y>=35 && y<55) {
            if(x<55)return {ActionType::PreviousLeds};
            if(x>=265)return {ActionType::NextLeds};
        }
        if (y >= 57 && y < 159) {
            int row=(y-57)/34, index=state.ledPage*3+row;
            if (index < (int)state.leds->size() && row<3) return {ActionType::SelectLed,index};
        }
        if (y >= 165 && y < 199) {
            if (x < 80) return {ActionType::TeamAway, -1};
            if (x < 161) return {ActionType::TeamHome, -1};
            if (x < 240) return {ActionType::PowerOff, -1};
            return {ActionType::ScanLeds, -1};
        }
    } else if (page_ == Page::Setup && y>=116&&y<140) {
        return {ActionType::ToggleResume};
    } else if (page_ == Page::Setup && y >= 169 && y < 200) {
        if(x>=160){page_=Page::Effects;return {};}
        return {ActionType::RefreshNhl, -1};
    } else if(page_==Page::Effects){
        if(y>=38&&y<67)return {ActionType::EffectsToggle};
        if(y>=73&&y<102)return {ActionType::EffectsNext};
        if(y>=108&&y<137)return {ActionType::EffectsEnable};
        if(y>=169&&y<198)return {x<160?ActionType::EffectsAway:ActionType::EffectsHome};
    } else if (page_ == Page::Audio) {
        if(y>=38&&y<67&&x>=180)return {ActionType::AudioToggle};
        if(y>=70&&y<101) {
            if(x>=105&&x<158)return {ActionType::AudioVolumeDown};
            if(x>=260)return {ActionType::AudioVolumeUp};
        }
        if(y>=107&&y<139&&x>=95)return {ActionType::AudioAwayNext};
        if(y>=143&&y<174&&x>=95)return {ActionType::AudioHomeNext};
        if(y>=175&&y<204)return {x<160?ActionType::AudioPreviewAway:ActionType::AudioPreviewHome};
    }
    return {};
}

void Ui::calibrationTarget(int index) {
    rendered_ = false;
    static const int x[] = {20, 300, 20}, y[] = {20, 20, 220};
    tft_.fillScreen(BG);
    tft_.setTextColor(FG, BG);
    tft_.drawCentreString("Tap and release each cross", 160, 90, 2);
    tft_.drawCentreString(String(index + 1) + " / 3", 160, 115, 2);
    tft_.drawLine(x[index]-10,y[index],x[index]+10,y[index],TFT_WHITE);
    tft_.drawLine(x[index],y[index]-10,x[index],y[index]+10,TFT_WHITE);
}
