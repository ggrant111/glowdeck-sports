#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
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
void redraw() { uiState.wifiConnected = WiFi.status() == WL_CONNECTED; uiState.ip = WiFi.localIP().toString(); ui.draw(uiState); lastUi = millis(); }
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
body{background:#0a1418;color:#eaf5f5;font:16px system-ui;margin:auto;max-width:620px;padding:24px}h1{color:#57bdb5}section{background:#182a30;border-radius:16px;padding:18px;margin:15px 0}input,button{border:0;border-radius:8px;padding:12px;font-size:15px;margin:4px}button{background:#31837a;color:white;cursor:pointer}input{max-width:170px}small{color:#a3b9bc}form{display:inline-block}</style></head><body><h1>GlowDeck GameDay v0.1</h1><p>Local setup for the ESP32 touchscreen sports controller.</p>)HTML";
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
    page += "</section><small>Prototype: local HTTP, no login. Use on a trusted Wi-Fi network only. Automatic game-event lighting is disabled.</small></body></html>";
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
    if (!ok) { banner(error); redraw(); return; }
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
    if (!tracked) uiState.selectedGameId = 0; // avoid accidentally following a different game
    if (uiState.gamePage * 4 >= scores.count) uiState.gamePage = 0;
    Serial.printf("NHL: %u games for %s\n", scores.count, scores.date);
    redraw();
}
void applyTeam(bool home) {
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
        uiState.selectedGameId = scores.games[a.index].id;
        banner(String("Tracking ") + scores.games[a.index].away + " @ " + scores.games[a.index].home);
        break;
      case ActionType::SelectLed: uiState.selectedLed = a.index; banner("WLED selected"); break;
      case ActionType::PreviousGames: if (uiState.gamePage > 0) --uiState.gamePage; break;
      case ActionType::NextGames: if ((uiState.gamePage+1)*4 < scores.count) ++uiState.gamePage; break;
      case ActionType::ScanLeds:
        wled.discover(); if (wled.size() && uiState.selectedLed < 0) uiState.selectedLed=0;
        banner(String("Found ") + wled.size() + " WLED device(s)"); break;
      case ActionType::TeamAway: applyTeam(false); break;
      case ActionType::TeamHome: applyTeam(true); break;
      case ActionType::PowerOff: {
        String err;
        if (uiState.selectedLed < 0) banner("Select a WLED device");
        else if (wled.setPower(uiState.selectedLed, false, err)) banner("Lights off");
        else banner(err);
        break;
      }
      case ActionType::RefreshNhl: refreshNhl(); break;
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
    if (!uiState.message.isEmpty() && millis() - bannerSet > 6000) uiState.message = "";
    if (millis() - lastUi > UI_INTERVAL_MS) redraw();
    delay(12);
}
