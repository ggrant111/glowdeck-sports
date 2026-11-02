#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "GoalEngine.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "GameCore.h"
#include "NhlClient.h"
#include "WledClient.h"
#include "Ui.h"

namespace {
constexpr uint32_t NHL_INTERVAL_MS = 60000;
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
WebServer web(80);
gd::Scoreboard scores;
uint32_t lastNhl = 0;
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
    uiState.favoriteHome = goals.favoriteHome();
    uiState.goalQueued = goals.pending();
    uiState.goalActive = reactions.active();
    uiState.restorePending = wled.autoRestorePending();
    ui.draw(uiState);
    lastUi = millis();
}
void saveGameSettings() {
    Preferences p;
    p.begin("gd-game", false);
    p.putUChar("delay", goals.delay());
    p.putBool("favHome", goals.favoriteHome());
    p.end();
}
void loadGameSettings() {
    Preferences p;
    p.begin("gd-game", true);
    goals.setDelay(p.getUChar("delay", 0));
    goals.setFavoriteHome(p.getBool("favHome", true));
    p.end();
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
void selectGame(uint32_t id) {
    if (!clearAutoBeforeManual()) return;
    uiState.selectedGameId = id;
    goals.selectGame(id); // disarms; sets a new fresh baseline on next arm
    const gd::Game *g = selectedGame();
    if (g) banner(String("Selected ") + g->away + " @ " + g->home);
    else banner("Choose a live game");
}
void setFavorite(bool home) {
    if (!clearAutoBeforeManual()) return;
    goals.setFavoriteHome(home);
    saveGameSettings();
    banner(home ? "Favorite: HOME" : "Favorite: AWAY");
}
void setGoalDelay(int delta) {
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
    banner("Goal lights ARMED");
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
body{background:#0a1418;color:#eaf5f5;font:16px system-ui;margin:auto;max-width:620px;padding:24px}h1{color:#57bdb5}section{background:#182a30;border-radius:16px;padding:18px;margin:15px 0}input,button{border:0;border-radius:8px;padding:12px;font-size:15px;margin:4px}button{background:#31837a;color:white;cursor:pointer}input{max-width:170px}small{color:#a3b9bc}form{display:inline-block}</style></head><body><h1>GlowDeck GameDay v0.2</h1><p>Local setup for the ESP32 touchscreen sports controller.</p>)HTML";
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
    page += "</section><section><h2>GameDay goal reactions</h2><p>Default OFF at every boot. Scoreboard-delta prototype; no authoritative play IDs.</p>";
    page += "<p>Goal lights: <strong>" + String(goals.armed() ? "ARMED" : "OFF") + "</strong> | " +
      "Favorite: " + String(goals.favoriteHome() ? "HOME" : "AWAY") +
      " | TV delay: " + String(goals.delay()) + "s</p>";
    page += "<p>Selected WLED: " + String(uiState.selectedLed >= 0 && (size_t)uiState.selectedLed < wled.size()
      ? htmlEscape(wled.at(uiState.selectedLed).name) : "none") + "</p>";
    if (wled.autoRestorePending()) page += "<p>Saved scene recovery pending; no new goal scenes allowed.</p>";
    page += "<form method='post' action='/gameday'><button name='action' value='toggle'>ARM / DISARM</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='away'>Favorite away</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='home'>Favorite home</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='minus'>Delay -15s</button></form>";
    page += "<form method='post' action='/gameday'><button name='action' value='plus'>Delay +15s</button></form>";
    page += "<h3>Select game</h3>";
    for (uint8_t i=0; i<scores.count; ++i) {
        const gd::Game &item = scores.games[i];
        page += "<form method='post' action='/selectgame'><button name='id' value='" + String(item.id) + "'>" +
            htmlEscape(String(item.away)+" @ "+item.home+" "+gd::gameStateText(item)) + "</button></form>";
    }
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
        goals.disarm(); // fail closed; reconnect requires deliberate rearm
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
    goals.observe(tracked, millis());
    if (!tracked) { uiState.selectedGameId = 0; goals.selectGame(0); }
    if (uiState.gamePage * 4 >= scores.count) uiState.gamePage = 0;
    Serial.printf("NHL: %u games for %s\n", scores.count, scores.date);
    redraw();
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
      case ActionType::FavoriteAway: setFavorite(false); break;
      case ActionType::FavoriteHome: setFavorite(true); break;
      case ActionType::DelayMinus: setGoalDelay(-15); break;
      case ActionType::DelayPlus: setGoalDelay(15); break;
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
        else if (a=="away") setFavorite(false);
        else if (a=="home") setFavorite(true);
        else if (a=="minus") setGoalDelay(-15);
        else if (a=="plus") setGoalDelay(15);
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
    }
    pollTouch();
    const bool online = WiFi.status() == WL_CONNECTED;
    const uint32_t started = reactions.startedCount(), restored = reactions.restoreCount();
    reactions.service(millis(), online);
    // Recovery must also work if NHL is unavailable or no WLED is selected.
    // See implementation: the driver should only depend on Wi-Fi for restore.
    if (reactions.startedCount() != started) banner("GOAL! Team color active");
    if (reactions.restoreCount() != restored) banner("Original lights restored");
    if (!uiState.message.isEmpty() && millis() - bannerSet > 6000) uiState.message = "";
    if (millis() - lastUi > UI_INTERVAL_MS) redraw();
    delay(12);
}
