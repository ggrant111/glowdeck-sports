#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "GoalEngine.h"
#include "AudioPlayer.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "GameCore.h"
#include "NhlClient.h"
#include "WledClient.h"
#include "Ui.h"

namespace {
constexpr uint32_t NHL_INTERVAL_MS = 60000;
constexpr uint32_t PBP_INTERVAL_MS = 25000;
constexpr uint32_t UI_INTERVAL_MS = 3000;
constexpr int TOUCH_CS = 33, TOUCH_IRQ = 36;
constexpr int TOUCH_CLK = 25, TOUCH_MISO = 39, TOUCH_MOSI = 32;
// Calibrate these constants if touch is mirrored/offset on your particular CYD board.
constexpr int RAW_X_MIN = 200, RAW_X_MAX = 3800;
constexpr int RAW_Y_MIN = 200, RAW_Y_MAX = 3800;
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
Ui ui;
UiState uiState;
NhlClient nhl;
WledClient wled;
gd::GoalEngine goals;
// No external computer: WLED on LAN implements journaled LightPort.
class WledLightPort : public gd::LightPort {
public:
    bool startColor(uint32_t color) override {
        if (uiState.selectedLed < 0 || (size_t)uiState.selectedLed >= wled.size()) return false;
        String err;
        bool ok = wled.beginAutoColor((size_t)uiState.selectedLed, color, err);
        if (!ok) Serial.println("Auto scene start: " + err);
        return ok;
    }
    bool restore() override {
        String err;
        bool ok = wled.restoreAuto(err);
        if (!ok) Serial.println("Auto scene recovery: " + err);
        return ok;
    }
    bool needsRestore() const override { return wled.autoRestorePending(); }
};
WledLightPort lightPort;
gd::ReactionController reactions(goals, lightPort);
AudioPlayer audio;
bool soundEnabled=false; // safe default OFF every boot
uint8_t soundVolume=50;
gd::AudioChoice awaySound, homeSound;
bool touchSuspendedForSd=false;

void restoreTouchBus() {
    if(!touchSuspendedForSd || audio.busy()) return;
    touchSpi.begin(TOUCH_CLK,TOUCH_MISO,TOUCH_MOSI,TOUCH_CS);
    touch.begin(touchSpi);
    touch.setRotation(1);
    touchSuspendedForSd=false;
}
void loadSoundForTeams();
bool trySound(const gd::AudioChoice &choice) {
    if(choice.kind==gd::SoundKind::Off)return false;
    if(choice.kind==gd::SoundKind::CustomWav) {
        if(touchSuspendedForSd) return false;
        touchSpi.end();touchSuspendedForSd=true; // VSPI pin-mux given temporarily to SD
    }
    bool ok=audio.play(choice,soundVolume);
    if(!ok){restoreTouchBus();Serial.println("Audio preview failed: "+audio.lastError());}
    return ok;
}

WebServer web(80);
gd::Scoreboard scores;
uint32_t lastNhl = 0, lastPbp = 0;
bool pbpEverPolled = false;
uint32_t lastUi = 0;
bool nhlEverPolled = false;
bool servicesStarted = false;
uint32_t lastTap = 0;
uint32_t bannerSet = 0;

const gd::Game* selectedGame() { return gd::findGame(scores, uiState.selectedGameId); }
void banner(const String &s) {
    uiState.message = s;
    bannerSet = millis();
    Serial.println(String("GameDay: ") + s);
}
void redraw() {
    uiState.wifiConnected = WiFi.status() == WL_CONNECTED;
    uiState.ip = WiFi.localIP().toString();
    uiState.goalArmed = goals.armed();
    uiState.goalDelay = goals.delay();
    uiState.celebrationMode = goals.mode();
    uiState.goalPaused = goals.paused();
    uiState.goalBaselineReady = goals.baselined();
    uiState.goalQueued = goals.pending();
    uiState.goalActive = reactions.active();
    uiState.restorePending = wled.autoRestorePending();
    uiState.soundEnabled=soundEnabled;
    uiState.soundBusy=audio.busy();
    uiState.soundVolume=soundVolume;
    uiState.awaySound=awaySound.kind;
    uiState.homeSound=homeSound.kind;
    ui.draw(uiState);
    lastUi = millis();
}
void saveGameSettings() {
    Preferences p;
    p.begin("gd-game", false);
    p.putUChar("delay", goals.delay());
    if (uiState.selectedGameId)
        p.putUChar((String("m")+uiState.selectedGameId).c_str(), (uint8_t)goals.mode());
    p.end();
}
void loadGameSettings() {
    Preferences p;
    p.begin("gd-game", true);
    goals.setDelay(p.getUChar("delay", 0));
    goals.setMode(gd::CelebrationMode::Away);
    p.end();
    Preferences soundPrefs;soundPrefs.begin("gd-sound",true);
    soundVolume=soundPrefs.getUChar("volume",50);
    if(soundVolume>100)soundVolume=50;
    soundPrefs.end();
    soundEnabled=false; // deliberate user opt-in on each boot
    // Never persist armed=true. Require a live feed and deliberate arm each boot.
}
// Manual actions restore the automatic scene FIRST, rather than silently
// destroying the saved original state. No manual mutation if restore fails.
bool clearAutoBeforeManual() {
    goals.disarm();
    if (!wled.autoRestorePending()) return true;
    String err;
    if (wled.restoreAuto(err)) return true;
    banner(err);
    return false;
}
void loadSoundForTeams() {
    awaySound=gd::AudioChoice();homeSound=gd::AudioChoice();
    const gd::Game* g=selectedGame();if(!g)return;
    Preferences p;p.begin("gd-sound",true);
    gd::AudioChoice* pair[2]={&awaySound,&homeSound};
    const char* codes[2]={g->away,g->home};
    for(int i=0;i<2;i++) {
        String code=String(codes[i]);
        pair[i]->kind=gd::validatedSound(p.getUChar(("k"+code).c_str(),1));
        String clip=p.getString(("f"+code).c_str(),"GOAL");
        if(!gd::validClipName(clip.c_str()))clip="GOAL";
        strncpy(pair[i]->clip,clip.c_str(),sizeof(pair[i]->clip)-1);
        pair[i]->clip[sizeof(pair[i]->clip)-1]=0;
    }
    p.end();
}
void saveSoundForTeam(bool home) {
    const gd::Game *g=selectedGame();if(!g)return;
    const char *team=home?g->home:g->away;
    const gd::AudioChoice &c=home?homeSound:awaySound;
    Preferences p;p.begin("gd-sound",false);
    p.putUChar(("k"+String(team)).c_str(),(uint8_t)c.kind);
    p.putString(("f"+String(team)).c_str(),c.clip);
    p.end();
}
void cycleSound(bool home) {
    gd::AudioChoice &c=home?homeSound:awaySound;
    const uint8_t next=((uint8_t)c.kind+1)%5;
    c.kind=gd::validatedSound(next);
    saveSoundForTeam(home);
    banner(String(home?"HOME":"AWAY")+" sound updated");
}
void toggleSound() {
    soundEnabled=!soundEnabled;
    if(!soundEnabled)audio.stop();
    banner(soundEnabled?"Sound enabled":"Sound muted");
}
void adjustVolume(int delta) {
    int n=(int)soundVolume+delta;
    soundVolume=(uint8_t)constrain(n,0,100);
    Preferences p;p.begin("gd-sound",false);p.putUChar("volume",soundVolume);p.end();
    banner("Sound volume "+String(soundVolume)+"%");
}
void previewSound(bool home) {
    const gd::AudioChoice &choice=home?homeSound:awaySound;
    if(!trySound(choice))banner(String("Audio: ")+audio.lastError());
    else banner(String("Sound preview: ")+(home?"HOME":"AWAY"));
}
void selectGame(uint32_t id) {
    if (goals.armed()) { banner("Disarm before changing game"); return; }
    if (!clearAutoBeforeManual()) return;
    uiState.selectedGameId = id;
    goals.selectGame(id);
    Preferences p; p.begin("gd-game",true);
    goals.setMode(gd::validatedMode(p.getUChar((String("m")+id).c_str(), 0)));
    p.end();
    loadSoundForTeams();
    const gd::Game *g = selectedGame();
    if (g) banner(String("Selected ") + g->away + " @ " + g->home);
    else banner("Choose a live game");
}
void setCelebrations(gd::CelebrationMode mode) {
    if (goals.armed()) { banner("Disarm before changing team mode"); return; }
    if (!clearAutoBeforeManual()) return;
    goals.setMode(mode);
    saveGameSettings();
    banner(String("Celebrate: ") + gd::celebrationName(goals.mode()));
}
void setGoalDelay(int delta) {
    if (goals.armed()) { banner("Disarm before changing delay"); return; }
    if (!clearAutoBeforeManual()) return;
    int next = (int)goals.delay() + delta;
    if (next < 0) next = 0;
    if (next > 90) next = 90;
    goals.setDelay((uint8_t)next);
    saveGameSettings();
    banner(String("TV delay ") + goals.delay() + " seconds");
}
void toggleArm() {
    if (goals.armed()) {
        goals.disarm();
        audio.stop();
        reactions.service(millis(), WiFi.status() == WL_CONNECTED);
        banner("GameDay disarmed");
        return;
    }
    const gd::Game *g = selectedGame();
    if (wled.autoRestorePending()) { banner("Restore WLED scene first"); return; }
    if (WiFi.status() != WL_CONNECTED || !uiState.nhlConnected ||
        !nhlEverPolled || (uint32_t)(millis()-lastNhl)>90000U) {
        banner("Refresh NHL before arming"); return;
    }
    if (uiState.selectedLed < 0 || (size_t)uiState.selectedLed >= wled.size()) {
        banner("Select a WLED device"); return;
    }
    if (!goals.arm(g, millis())) { banner("Choose a LIVE game first"); return; }
    reactions.setTeamColors(gd::teamColor(g->away), gd::teamColor(g->home));
    pbpEverPolled=false;
    banner("Goal monitor: fetching play baseline");
}

String htmlEscape(const String &s) {
    String out;
    for (size_t i=0; i<s.length(); ++i) {
        switch (s[i]) {
          case '&': out += "&amp;"; break;
          case '<': out += "&lt;"; break;
          case '>': out += "&gt;"; break;
          case '"': out += "&quot;"; break;
          case '\'': out += "&#39;"; break;
          default: out += s[i];
        }
    }
    return out;
}
void redirectHome() { web.sendHeader("Location", "/", true); web.send(303, "text/plain", "Redirecting"); }
void webHome() {
    String page = R"HTML(<!doctype html><html lang="en"><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>GlowDeck GameDay</title><style>
body{background:#0a1418;color:#eaf5f5;font:16px system-ui;margin:auto;max-width:620px;padding:24px}h1{color:#57bdb5}section{background:#182a30;border-radius:16px;padding:18px;margin:15px 0}input,button{border:0;border-radius:8px;padding:12px;font-size:15px;margin:4px}button{background:#31837a;color:white;cursor:pointer}input{max-width:170px}small{color:#a3b9bc}form{display:inline-block}</style></head><body><h1>GlowDeck GameDay v0.4</h1><p>Local setup for the ESP32 touchscreen sports controller.</p>)HTML";
    page += "<section><h2>Status</h2><p>Wi-Fi: " + htmlEscape(WiFi.SSID()) + "<br>NHL: ";
    page += uiState.nhlConnected ? "connected" : "unavailable";
    page += "<br>Games today: " + String(scores.count) + "</p><form method='post' action='/refresh'><button>Refresh NHL</button></form></section>";
    page += "<section><h2>WLED devices</h2><p>Add a WLED device by numeric IPv4 address (must be on this LAN).</p>";
    page += "<form method='post' action='/add'><input name='ip' placeholder='192.168.1.71' required maxlength='15'><button>Add device</button></form>";
    page += "<form method='post' action='/scan'><button>Scan mDNS</button></form>";
    for (size_t i=0; i<wled.size(); ++i) {
        const LedDevice &d = wled.at(i);
        page += "<p>" + htmlEscape(d.name) + " (" + htmlEscape(d.ip) + ") ";
        page += "<form method='post' action='/remove'><input type='hidden' name='ip' value='"+htmlEscape(d.ip)+"'><button>Remove</button></form></p>";
    }
    page += "</section><section><h2>Team colors</h2><p>Select a game on the touchscreen first, then select a WLED device.</p>";
    const gd::Game *g = selectedGame();
    if (g) {
        page += "<p>Selected: " + String(g->away) + " at " + String(g->home) + "</p>";
        page += "<form method='post' action='/apply'><input type='hidden' name='side' value='away'><button>Apply away team</button></form>";
        page += "<form method='post' action='/apply'><input type='hidden' name='side' value='home'><button>Apply home team</button></form>";
    } else page += "<p>No game selected.</p>";
    page += "</section><section><h2>GameDay goal reactions</h2><p>Default OFF at every boot. NHL play-by-play goal event IDs; first response is baseline only.</p>";
    page += "<p>Goal lights: <strong>" + String(goals.armed() ? "ARMED" : "OFF") + "</strong> | " +
      "Celebrate: " + String(gd::celebrationName(goals.mode())) +
      " | TV delay: " + String(goals.delay()) + "s</p>";
    page += "<p>Selected WLED: " + String(uiState.selectedLed >= 0 && (size_t)uiState.selectedLed < wled.size()
      ? htmlEscape(wled.at(uiState.selectedLed).name) : "none") + "</p>";
    if (wled.autoRestorePending()) page += "<p>Saved scene recovery pending; no new goal scenes allowed.</p>";
    page += "<form method='post' action='/gameday'><button name='action' value='toggle'>ARM / DISARM</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='away'>Celebrate away</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='home'>Celebrate home</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='both'>Celebrate BOTH</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='minus'>Delay -15s</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='plus'>Delay +15s</button></form>";
    page += "<h3>Select game</h3>";
    for (uint8_t i=0; i<scores.count; ++i) {
        const gd::Game &item = scores.games[i];
        page += "<form method='post' action='/selectgame'><button name='id' value='" + String(item.id) + "'>" +
            htmlEscape(String(item.away)+" @ "+item.home+" "+gd::gameStateText(item)) + "</button></form>";
    }
    page += "</section><section><h2>GameDay Sounds (v0.4)</h2><p>Built-in horns and SD WAV files. Sound OFF after reboot. Preview works without NHL/WLED.</p>";
    page += "<p>Audio: " + String(soundEnabled?"ON":"OFF") + " | Volume: " + String(soundVolume) + "%</p>";
    page += "<form method='post' action='/sound'><button name='action' value='toggle'>Sound ON/OFF</button></form>";
    page += "<form method='post' action='/sound'><button name='action' value='down'>Volume -10</button></form>";
    page += "<form method='post' action='/sound'><button name='action' value='up'>Volume +10</button></form>";
    auto soundName=[](gd::SoundKind k)->String{switch(k){
        case gd::SoundKind::Horn:return "Horn 1";case gd::SoundKind::DoubleHorn:return "Horn 2";
        case gd::SoundKind::Siren:return "Horn 3";case gd::SoundKind::CustomWav:return "Custom WAV";
        default:return "Off";
    }};
    page += "<p>Away: "+soundName(awaySound.kind)+" ("+htmlEscape(awaySound.clip)+")</p>";
    page += "<p>Home: "+soundName(homeSound.kind)+" ("+htmlEscape(homeSound.clip)+")</p>";
    page += "<form method='post' action='/sound'><button name='action' value='away'>Next away sound</button></form>";
    page += "<form method='post' action='/sound'><button name='action' value='home'>Next home sound</button></form>";
    page += "<form method='post' action='/sound'><button name='action' value='previewAway'>Preview away</button></form>";
    page += "<form method='post' action='/sound'><button name='action' value='previewHome'>Preview home</button></form>";
    page += "<p>Custom file name: /audio/NAME.wav (mono PCM 16 kHz, 16 bit, up to 10 seconds)</p>";
    page += "<form method='post' action='/sound'><input name='clip' placeholder='GOAL' maxlength='24' required pattern='[A-Z0-9_-]+'><button name='action' value='fileAway'>Set away WAV</button><button name='action' value='fileHome'>Set home WAV</button></form>";
    page += "</section><small>Prototype: local HTTP without login, trusted LAN only. Do not port forward. Solid colors only; WLED segment 0 auto-snapshot/restore.</small></body></html>";
    web.send(200, "text/html; charset=utf-8", page);
}
void refreshNhl() {
    if (WiFi.status() != WL_CONNECTED) { banner("No Wi-Fi for NHL"); return; }
    gd::Scoreboard incoming;
    String error;
    bool ok = nhl.poll(incoming, error);
    lastNhl = millis();
    nhlEverPolled = true;
    uiState.nhlConnected = ok;
    uiState.nhlError = error;
    if (!ok) {
        goals.pause(); // fail closed; rebaseline upon feed recovery
        banner(error); redraw(); return;
    }
    const gd::Game *old = selectedGame();
    gd::Game previous;
    bool hadPrevious = old != nullptr;
    if (old) previous = *old;
    scores = incoming;
    const gd::Game *tracked = selectedGame();
    if (hadPrevious && tracked) {
        gd::GoalChange change = gd::detectGoalChange(previous, *tracked);
        if (change.changed) banner(String("SCORE! Away +") + change.awayAdded + " Home +" + change.homeAdded);
    }
    goals.observeGame(tracked);
    if (!tracked) { uiState.selectedGameId = 0; goals.selectGame(0); }
    if (uiState.gamePage * 4 >= scores.count) uiState.gamePage = 0;
    Serial.printf("NHL: %u games for %s\n", scores.count, scores.date);
    redraw();
}
void refreshPlayByPlay() {
    if (!goals.armed() || WiFi.status()!=WL_CONNECTED) return;
    gd::GoalFeed sample; String error;
    const bool wasPaused=goals.paused();
    const bool ok=nhl.pollGoals(goals.gameId(),sample,error);
    lastPbp=millis(); pbpEverPolled=true;
    if (!ok) {
        goals.pause();
        banner(String("NHL PBP: ")+error);
        return;
    }
    goals.observeFeed(sample,millis());
    if (wasPaused && goals.baselined()) banner("NHL play baseline ready");
    Serial.printf("PBP game=%lu latest=%lu goals=%u %s\n",
        (unsigned long)sample.gameId,(unsigned long)sample.latestEventId,
        sample.count,goals.paused()?"PAUSED":"ARMED");
}
void applyTeam(bool home) {
    if (!clearAutoBeforeManual()) return;
    const gd::Game *g = selectedGame();
    if (!g) { banner("Select a game first"); return; }
    if (uiState.selectedLed < 0 || (size_t)uiState.selectedLed >= wled.size()) {
        banner("Select a WLED device first"); return;
    }
    const char *team = home ? g->home : g->away;
    String err;
    if (wled.applyColor(uiState.selectedLed, gd::teamColor(team), err)) banner(String(team) + " color sent");
    else banner(err);
}
void doAction(const UiAction &a) {
    switch (a.type) {
      case ActionType::SelectGame:
        if (a.index >= 0 && a.index < scores.count) selectGame(scores.games[a.index].id);
        break;
      case ActionType::SelectLed:
        if (clearAutoBeforeManual()) { uiState.selectedLed = a.index; banner("WLED selected"); }
        break;
      case ActionType::PreviousGames: if (uiState.gamePage > 0) --uiState.gamePage; break;
      case ActionType::NextGames: if ((uiState.gamePage+1)*4 < scores.count) ++uiState.gamePage; break;
      case ActionType::ScanLeds:
        wled.discover(); if (wled.size() && uiState.selectedLed < 0) uiState.selectedLed=0;
        banner(String("Found ") + wled.size() + " WLED device(s)"); break;
      case ActionType::TeamAway: applyTeam(false); break;
      case ActionType::TeamHome: applyTeam(true); break;
      case ActionType::PowerOff: {
        if (!clearAutoBeforeManual()) break;
        String err;
        if (uiState.selectedLed < 0) banner("Select a WLED device");
        else if (wled.setPower(uiState.selectedLed, false, err)) banner("Lights off");
        else banner(err);
        break;
      }
      case ActionType::RefreshNhl: refreshNhl(); break;
      case ActionType::ToggleArm: toggleArm(); break;
      case ActionType::CelebrateAway: setCelebrations(gd::CelebrationMode::Away); break;
      case ActionType::CelebrateHome: setCelebrations(gd::CelebrationMode::Home); break;
      case ActionType::CelebrateBoth: setCelebrations(gd::CelebrationMode::Both); break;
      case ActionType::DelayMinus: setGoalDelay(-15); break;
      case ActionType::DelayPlus: setGoalDelay(15); break;
      case ActionType::AudioToggle: toggleSound(); break;
      case ActionType::AudioVolumeDown: adjustVolume(-10); break;
      case ActionType::AudioVolumeUp: adjustVolume(10); break;
      case ActionType::AudioAwayNext: cycleSound(false); break;
      case ActionType::AudioHomeNext: cycleSound(true); break;
      case ActionType::AudioPreviewAway: previewSound(false); break;
      case ActionType::AudioPreviewHome: previewSound(true); break;
      default: break;
    }
    redraw();
}
void setupWeb() {
    if (servicesStarted) return;
    web.on("/", HTTP_GET, webHome);
    web.on("/add", HTTP_POST, [] {
        String err;
        if (wled.add(web.arg("ip"), err)) {
            if (uiState.selectedLed < 0) uiState.selectedLed = 0;
            banner("WLED added");
        } else banner(err);
        redirectHome();
    });
    web.on("/remove", HTTP_POST, [] {
        if (wled.remove(web.arg("ip"))) { uiState.selectedLed = wled.size() ? 0 : -1; banner("Device removed"); }
        redirectHome();
    });
    web.on("/scan", HTTP_POST, [] { wled.discover(); if (wled.size() && uiState.selectedLed<0) uiState.selectedLed=0; banner("Discovery complete"); redirectHome(); });
    web.on("/refresh", HTTP_POST, [] { refreshNhl(); redirectHome(); });
    web.on("/apply", HTTP_POST, [] { applyTeam(web.arg("side")=="home"); redirectHome(); });
    web.on("/selectgame", HTTP_POST, [] {
        const uint32_t id = (uint32_t)web.arg("id").toInt();
        if (gd::findGame(scores,id)) selectGame(id);
        redirectHome();
    });
    web.on("/gameday", HTTP_POST, [] {
        String a = web.arg("action");
        if (a=="toggle") toggleArm();
        else if (a=="away") setCelebrations(gd::CelebrationMode::Away);
        else if (a=="home") setCelebrations(gd::CelebrationMode::Home);
        else if (a=="both") setCelebrations(gd::CelebrationMode::Both);
        else if (a=="minus") setGoalDelay(-15);
        else if (a=="plus") setGoalDelay(15);
        redirectHome();
    });
    web.on("/sound",HTTP_POST,[]{
        String action=web.arg("action");
        if(action=="toggle")toggleSound();
        else if(action=="down")adjustVolume(-10);
        else if(action=="up")adjustVolume(10);
        else if(action=="away")cycleSound(false);
        else if(action=="home")cycleSound(true);
        else if(action=="previewAway")previewSound(false);
        else if(action=="previewHome")previewSound(true);
        else if(action=="fileAway"||action=="fileHome"){
            String name=web.arg("clip");
            if(gd::validClipName(name.c_str())){
                bool home=action=="fileHome";
                gd::AudioChoice &choice=home?homeSound:awaySound;
                choice.kind=gd::SoundKind::CustomWav;
                strncpy(choice.clip,name.c_str(),sizeof(choice.clip)-1);
                choice.clip[sizeof(choice.clip)-1]=0;
                saveSoundForTeam(home);
                banner("Custom sound: "+name);
            } else banner("Invalid WAV name; uppercase A-Z, 0-9, - or _ only");
        }
        redirectHome();
    });
    web.onNotFound([] { web.send(404, "text/plain", "Not found"); });
    web.begin();
    servicesStarted = true;
}
void initWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFiManager wm;
    wm.setConfigPortalTimeout(180);
    // Holding BOOT while powering on clears ONLY Wi-Fi credentials, not WLED devices.
    pinMode(0, INPUT_PULLUP);
    if (digitalRead(0) == LOW) {
        wm.resetSettings();
        banner("Wi-Fi cleared: join setup AP");
    }
    banner("Joining Wi-Fi / captive portal...");
    redraw();
    if (wm.autoConnect("GlowDeck-GameDay")) {
        Serial.printf("Wi-Fi IP: %s\n", WiFi.localIP().toString().c_str());
        if (MDNS.begin("glowdeck-gameday")) MDNS.addService("http", "tcp", 80);
        setupWeb();
        wled.discover();
        if (wled.size()) uiState.selectedLed = 0;
        banner("Wi-Fi connected");
    } else banner("Wi-Fi setup timed out. Reboot to retry.");
}
void pollTouch() {
    if (touchSuspendedForSd) return;
    if (!touch.touched() || millis() - lastTap < 230) return;
    TS_Point p = touch.getPoint();
    // XPT2046 setRotation(1) reports a rotated raw space for landscape.
    int x = constrain(map(p.x, RAW_X_MIN, RAW_X_MAX, 0, 320), 0, 319);
    int y = constrain(map(p.y, RAW_Y_MIN, RAW_Y_MAX, 0, 240), 0, 239);
    Serial.printf("Touch raw(%d,%d) screen(%d,%d)\n", p.x, p.y, x, y);
    lastTap = millis();
    doAction(ui.touch(x, y, uiState));
}
}

void setup() {
    Serial.begin(115200);
    delay(250);
    uiState.scoreboard = &scores;
    uiState.leds = &wled;
    ui.begin();
    touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin(touchSpi);
    touch.setRotation(1);
    wled.begin();
    if(!audio.begin())Serial.println("Audio hardware setup: "+audio.lastError());
    loadGameSettings();
    if (wled.size()) uiState.selectedLed = 0;
    redraw();
    initWiFi();
    if (WiFi.status() == WL_CONNECTED) refreshNhl();
    redraw();
}
void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        // Handle a Wi-Fi connection established after the captive portal timed out.
        if (!servicesStarted) {
            if (MDNS.begin("glowdeck-gameday")) MDNS.addService("http", "tcp", 80);
            setupWeb();
        }
        web.handleClient();
        if (!nhlEverPolled || millis() - lastNhl >= NHL_INTERVAL_MS) refreshNhl();
        if (goals.armed() && (!pbpEverPolled || millis()-lastPbp>=PBP_INTERVAL_MS)) refreshPlayByPlay();
    }
    restoreTouchBus();
    pollTouch();
    const bool online = WiFi.status() == WL_CONNECTED;
    const uint32_t started = reactions.startedCount(), restored = reactions.restoreCount();
    reactions.service(millis(), online);
    // Recovery must also work if NHL is unavailable or no WLED is selected.
    // See implementation: the driver should only depend on Wi-Fi for restore.
    if (reactions.startedCount() != started) {
        banner("GOAL! Team color active");
        if(soundEnabled)trySound(reactions.lastGoalHome()?homeSound:awaySound);
    }
    if(reactions.restoreCount()!=restored)audio.stop();
    if (reactions.restoreCount() != restored) banner("Original lights restored");
    if (!uiState.message.isEmpty() && millis() - bannerSet > 6000) uiState.message = "";
    if (millis() - lastUi > UI_INTERVAL_MS) redraw();
    delay(12);
}
