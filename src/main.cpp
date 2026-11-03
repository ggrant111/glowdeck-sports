#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "GoalEngine.h"
#include "FollowManager.h"
#include "AudioPlayer.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "GameCore.h"
#include "NhlClient.h"
#include "WledClient.h"
#include "Ui.h"
#include "TouchTransform.h"

// TLS handshakes run inside setup/loop and need headroom beyond the default 8 KiB.
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

namespace {
constexpr uint32_t NHL_INTERVAL_MS = 60000;
constexpr uint32_t PBP_INTERVAL_MS = 25000;
constexpr uint32_t UI_INTERVAL_MS = 1000;
constexpr int TOUCH_CS = 33, TOUCH_IRQ = 36;
constexpr int TOUCH_CLK = 25, TOUCH_MISO = 39, TOUCH_MOSI = 32;
// Calibrate these constants if touch is mirrored/offset on your particular CYD board.
gd::TouchTransform touchMap;
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
Ui ui;
UiState uiState;
NhlClient nhl;
WledClient wled;
FollowManager following;
gd::GoalEngine goals;
uint8_t effectMask=127,effectSelection=0;
bool effectPreview=false;
uint32_t effectPreviewEnds=0;
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
    bool startEvent(uint32_t color,gd::SportEvent kind) override {
        if(uiState.selectedLed<0)return false;
        String error;bool ok=wled.beginAutoEvent(uiState.selectedLed,color,kind,error);
        if(!ok)Serial.println(error);return ok;
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
gd::Scoreboard scores,todayScores;
bool todayOnline=false;
gd::Game trackedGame;
uint8_t scheduleDay=0;
bool hour24=true;
bool resumeEnabled=false,resumePending=false;
String resumeIp;
uint32_t lastNhl = 0, lastPbp = 0;
bool pbpEverPolled = false;
uint32_t lastUi = 0;
bool nhlEverPolled = false;
bool servicesStarted = false;
uint32_t lastTap = 0;
uint32_t bannerSet = 0;

const gd::Game* selectedGame() { return trackedGame.id==uiState.selectedGameId && trackedGame.id ? &trackedGame : nullptr; }
void saveResume();
void banner(const String &s) {
    uiState.message = s;
    bannerSet = millis();
    Serial.println(String("GameDay: ") + s);
}
void redraw() {
    uiState.today=&todayScores;uiState.todayOnline=todayOnline && WiFi.status()==WL_CONNECTED;
    uiState.hour24=hour24;
    uiState.tracked=selectedGame();uiState.scheduleDay=scheduleDay;uiState.resumeEnabled=resumeEnabled;
    uiState.ddp=wled.ddpEnabled;uiState.effectSelection=effectSelection;uiState.effectMask=effectMask;
    uiState.wifiConnected = WiFi.status() == WL_CONNECTED;
    uiState.ip = WiFi.localIP().toString();
    uiState.goalArmed = goals.armed();
    uiState.waiting = goals.armed() && goals.waiting();
    uiState.ledPage = min(uiState.ledPage,max(0,(int(wled.size())-1)/3));
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
    wled.ddpEnabled=p.getBool("ddp",false);effectMask=p.getUChar("events",127)&127;
    goals.setEventMask(wled.ddpEnabled?effectMask:1,wled.ddpEnabled);
    goals.setDelay(p.getUChar("delay", 0));
    goals.setMode(gd::CelebrationMode::Away);
    p.end();
    Preferences soundPrefs;soundPrefs.begin("gd-sound",true);
    soundVolume=soundPrefs.getUChar("volume",50);
    if(soundVolume>100)soundVolume=50;
    soundPrefs.end();
    soundEnabled=false; // deliberate user opt-in on each boot
    Preferences resume;
    if(resume.begin("gd-resume",false)) {
        hour24=resume.getBool("hour24",true);
        resumeEnabled=resume.getBool("enabled",false);
        resumePending=resumeEnabled&&resume.getBool("armed",false);
        if(resumePending && resume.getBytesLength("game")==sizeof(trackedGame)) {
            resume.getBytes("game",&trackedGame,sizeof(trackedGame));
            uiState.selectedGameId=trackedGame.id;goals.selectGame(trackedGame.id);
            goals.setMode(gd::validatedMode(resume.getUChar("mode",0)));resumeIp=resume.getString("ip","");
        } else resumePending=false;
        String zone=resume.getString("tz","UTC0");setenv("TZ",zone.c_str(),1);tzset();
        resume.end();
    }
}
// Manual actions restore the automatic scene FIRST, rather than silently
// destroying the saved original state. No manual mutation if restore fails.
bool clearAutoBeforeManual() {
    if(following.busy()){banner("Stop followed games / wait for restoration first");return false;}
    goals.disarm();effectPreview=false;resumePending=false;saveResume();
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
    if(following.busy()){banner("Stop followed games before sound preview");return;}
    const gd::AudioChoice &choice=home?homeSound:awaySound;
    if(!trySound(choice))banner(String("Audio: ")+audio.lastError());
    else banner(String("Sound preview: ")+(home?"HOME":"AWAY"));
}
void selectGame(uint32_t id) {
    if (goals.armed()) { banner("Disarm before changing game"); return; }
    if (!clearAutoBeforeManual()) return;
    const gd::Game *chosen=gd::findGame(scores,id);
    if(!chosen)return;
    trackedGame=*chosen;
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
void saveResume() {
    Preferences p;if(!p.begin("gd-resume",false))return;
    p.putBool("enabled",resumeEnabled);p.putBool("armed",goals.armed());
    if(goals.armed()&&selectedGame()&&uiState.selectedLed>=0) {
        p.putBytes("game",&trackedGame,sizeof(trackedGame));p.putString("ip",wled.at(uiState.selectedLed).ip);
        p.putUChar("mode",uint8_t(goals.mode()));
    }
    p.end();
}
void toggleArm() {
    if(following.busy()){banner("Stop followed games first");return;}
    if(effectPreview){banner("Finish effect preview first");return;}
    if (resumePending) {resumePending=false;saveResume();banner("Reboot resume cancelled");return;}
    if (goals.armed()) {
        goals.disarm();
        audio.stop();
        reactions.service(millis(), WiFi.status() == WL_CONNECTED);
        saveResume();
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
    if (!goals.arm(g, millis())) { banner("Choose an upcoming or live game"); return; }
    reactions.setTeamColors(gd::teamColor(g->away), gd::teamColor(g->home));
    saveResume();
    pbpEverPolled=false;
    banner(goals.waiting()?"ARMED - waiting for game start":"Goal monitor: fetching play baseline");
}

void effectAction(int action,bool home=false){
    if(following.busy()){banner("Stop followed games first");return;}
    if(goals.armed()||effectPreview||wled.autoRestorePending()){banner("Stop / restore before changing effects");return;}
    if(action==0)wled.ddpEnabled=!wled.ddpEnabled;
    else if(action==1)effectSelection=(effectSelection+1)%7;
    else if(action==2)effectMask^=(1U<<effectSelection);
    else if(action==3){
        if(!wled.ddpEnabled||uiState.selectedLed<0){banner("Enable DDP and select WLED first");return;}
        const gd::Game *g=selectedGame();uint32_t color=gd::teamColor(g?(home?g->home:g->away):(home?"BOS":"UTA"));
        String error;
        if(wled.beginAutoEvent(uiState.selectedLed,color,gd::SportEvent(effectSelection),error)){
            effectPreview=true;effectPreviewEnds=millis()+8000;banner("DDP preview: "+String(gd::eventName(gd::SportEvent(effectSelection))));
        }else banner(error);
        return;
    }
    goals.setEventMask(wled.ddpEnabled?effectMask:1,wled.ddpEnabled);
    Preferences p;p.begin("gd-game",false);p.putBool("ddp",wled.ddpEnabled);p.putUChar("events",effectMask);p.end();
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
String webPageName(){
    String name=web.hasArg("return")?web.arg("return"):web.arg("page");
    const char *allowed[]={"games","following","lights","sound","setup"};
    for(auto candidate:allowed)if(name==candidate)return name;
    return "games";
}
void redirectHome() { web.sendHeader("Location", "/?page="+webPageName(), true); web.send(303, "text/plain", "Redirecting"); }
bool followSelected(){
    if(following.busy()){banner("Stop followed games before editing");return false;}
    const gd::Game *g=selectedGame();if(!g){banner("Select a game first");return false;}
    for(unsigned i=0;i<following.count;++i)if(following.games[i].game.id==g->id){banner("Already followed");return false;}
    if(following.count==gd::FOLLOW_LIMIT){banner("Four-game limit; remove a game first");return false;}
    auto &f=following.games[following.count++];f=FollowGame();f.game=*g;f.mode=goals.mode();f.delay=goals.delay();f.ddp=wled.ddpEnabled;f.mask=effectMask;
    if(!following.save())banner("Could not save following preferences");else banner("Game followed; configure outputs on local web page");return true;
}
void startFollowing(){
    if(goals.armed()||effectPreview||wled.autoRestorePending()){banner("Stop single-game monitoring / restore first");return;}
    resumePending=false;resumeEnabled=false;saveResume();String err;
    if(!following.start(err))banner(err);else banner("Followed games started");
}
String followingHtml(){
    String p="<section><h2>Followed games (v0.6)</h2><p>Up to four games, two independent WLED targets. Settings save; start after reboot. Sound also requires the SOUND master switch. Stop before editing.</p><p>"+htmlEscape(following.message)+"</p>";
    p+="<form method='post' action='/follow'><button name='op' value='add'>Follow selected game</button><button name='op' value='start'>Start all</button><button name='op' value='stop'>Stop all</button></form>";
    p+="<form method='post' action='/follow'><input type='hidden' name='op' value='targets'>";
    for(unsigned t=0;t<2;++t)p+=String(t?"B":"A")+" <input name='t"+t+"' value='"+htmlEscape(following.targetIp[t])+"' placeholder='WLED IPv4'>";
    p+="<button>Save targets</button></form>";
    for(unsigned i=0;i<following.count;++i){auto &f=following.games[i];
        p+="<h3>"+String(f.game.away)+" @ "+f.game.home+"</h3><p>"+htmlEscape(f.status)+"</p><form method='post' action='/follow'><input type='hidden' name='i' value='"+i+"'>";
        const char *names[]={"display","sound","a","b","ddp"};const char *labels[]={"Device display","Goal sound","Target A","Target B","DDP effects"};
        bool enabled[]={f.display,f.sound,bool(f.targets&1),bool(f.targets&2),f.ddp};
        for(int n=0;n<5;++n)p+="<label><input type='checkbox' name='"+String(names[n])+"'"+(enabled[n]?" checked":"")+">"+labels[n]+"</label> ";
        p+="<p>Celebrate <select name='mode'>";for(int m=0;m<3;++m)p+="<option value='"+String(m)+"'"+(int(f.mode)==m?" selected":"")+">"+gd::celebrationName(gd::validatedMode(m))+"</option>";
        p+="</select> Delay <input type='number' min='0' max='90' name='delay' value='"+String(f.delay)+"'> seconds</p>";
        for(unsigned e=0;e<7;++e)p+="<label><input type='checkbox' name='e"+String(e)+"'"+((f.mask&(1<<e))?" checked":"")+">"+gd::eventName(gd::SportEvent(e))+"</label> ";
        p+="<button name='op' value='save'>Save game</button><button name='op' value='remove'>Unfollow</button></form>";
    }return p+"</section>";
}
void webHome() {
    String page = R"HTML(<!doctype html><html lang="en"><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>GlowDeck GameDay</title><style>
body{background:#0a1418;color:#eaf5f5;font:16px system-ui;margin:auto;max-width:620px;padding:24px}h1{color:#57bdb5}section{background:#182a30;border-radius:16px;padding:18px;margin:15px 0}input,button{border:0;border-radius:8px;padding:12px;font-size:15px;margin:4px}button{background:#31837a;color:white;cursor:pointer}input{max-width:170px}small{color:#a3b9bc}form{display:inline-block}nav{display:flex;flex-wrap:wrap;gap:8px;position:sticky;top:0;background:#0a1418;padding:12px 0}nav a{padding:12px;color:#eaf5f5;text-decoration:none;border-radius:8px;background:#182a30}nav a[aria-current]{background:#31837a}a{color:#57bdb5}select{padding:10px;max-width:100%}button:focus-visible,a:focus-visible{outline:3px solid #f4c95d}footer{padding:24px 0}</style></head><body><h1>GlowDeck GameDay v0.8</h1><p>Local setup for the ESP32 touchscreen sports controller.</p>)HTML";
    const String current=webPageName();
    page+="<nav aria-label='Main navigation'>";
    const char *pages[]={"games","following","lights","sound","setup"};
    const char *labels[]={"Games","Following","Lights","Sound","Setup"};
    for(int i=0;i<5;++i)page+="<a href='/?page="+String(pages[i])+"'"+(current==pages[i]?" aria-current='page'":"")+">"+labels[i]+"</a>";
    page+="</nav><p role='status'>"+htmlEscape(uiState.message)+"</p>";
    if(current=="following"){
    page += followingHtml();
    }
    if(current=="setup"){
        page+="<section><h2>Preferences</h2>";
    page+="<form method='post' action='/timezone'><select name='tz'><option value='UTC0'>UTC</option><option value='MST7MDT,M3.2.0,M11.1.0'>Mountain</option><option value='PST8PDT,M3.2.0,M11.1.0'>Pacific</option><option value='CST6CDT,M3.2.0,M11.1.0'>Central</option><option value='EST5EDT,M3.2.0,M11.1.0'>Eastern</option></select><button>Save timezone</button></form>";
    page+="<form method='post' action='/timeformat'><label>Time format <select name='format'><option value='12'"+String(!hour24?" selected":"")+">12-hour (7:00 PM)</option><option value='24'"+String(hour24?" selected":"")+">24-hour (19:00)</option></select></label><button>Save time format</button></form>";
    page+="<form method='post' action='/resume'><button>Resume after reboot: "+String(resumeEnabled?"ON":"OFF")+"</button></form>";
        page+="</section>";
    }
    if(current=="games"){
    page += "<section><h2>Schedule: next 7 days</h2>";
    for(int day=0;day<7;++day)page+="<form method='post' action='/schedule'><button name='day' value='"+String(day)+"'>"+(day==0?String("Today"):day==1?String("Tomorrow"):String("Day +")+day)+"</button></form>";
    page+="<p>Schedule date: "+String(scores.date)+"</p>";
    page+="</section>";
    }
    if(current=="setup"){
    page += "<section><h2>Status</h2><p>Wi-Fi: " + htmlEscape(WiFi.SSID()) + "<br>NHL: ";
    page += uiState.nhlConnected ? "connected" : "unavailable";
    page += "<br>Games on selected date: " + String(scores.count) + "</p><form method='post' action='/refresh'><button>Refresh NHL</button></form>";
    page+="</section>";
    }
    if(current=="lights"){
    page += "<section><h2>WLED devices</h2><p>Add a WLED device by numeric IPv4 address (must be on this LAN).</p>";
    page += "<form method='post' action='/add'><input name='ip' placeholder='192.168.1.71' required maxlength='15'><button>Add device</button></form>";
    page += "<form method='post' action='/scan'><button>Scan mDNS</button></form>";
    for (size_t i=0; i<wled.size(); ++i) {
        const LedDevice &d = wled.at(i);
        page += "<form method='post' action='/selectled'><button name='index' value='"+String(i)+"'>Select</button></form>";
        page += "<p>" + htmlEscape(d.name) + " (" + htmlEscape(d.ip) + ") ";
        page += "<form method='post' action='/remove'><input type='hidden' name='ip' value='"+htmlEscape(d.ip)+"'><button>Remove</button></form></p>";
    }
    page+="</section>";
    }
    if(current=="lights"){
    page += "<section><h2>Team colors</h2><p>Select a game on the touchscreen first, then select a WLED device.</p>";
    const gd::Game *g = selectedGame();
    if (g) {
        page += "<p>Selected: " + String(g->away) + " at " + String(g->home) + "</p>";
        page += "<form method='post' action='/apply'><input type='hidden' name='side' value='away'><button>Apply away team</button></form>";
        page += "<form method='post' action='/apply'><input type='hidden' name='side' value='home'><button>Apply home team</button></form>";
    } else page += "<p>No game selected.</p>";
    page+="</section>";
    }
    if(current=="games"){
    page += "<section><h2>GameDay goal reactions</h2><p>Reboot resume is OFF unless enabled. Upcoming games wait without changing lights; first live connection skips historical events.</p>";
    page += "<p>Goal lights: <strong>" + String(goals.armed() ? (goals.waiting()?"ARMED - WAITING FOR START":"MONITORING") : "OFF") + "</strong> | " +
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
    page+="</section>";
    }
    if(current=="sound"){
    page += "<section><h2>GameDay Sounds (v0.4)</h2><p>Built-in horns and SD WAV files. Sound OFF after reboot. Preview works without NHL/WLED.</p>";
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
    page+="</section>";
    }
    if(current=="lights"){
    page += "<section><h2>DDP realtime event effects</h2><p>Whole device, 1–600 LEDs, 20fps, 8 seconds. Enable WLED realtime receive; Main segment only must be OFF.</p>";
    page += "<form method='post' action='/effects'><button name='action' value='0'>DDP "+String(wled.ddpEnabled?"ON":"OFF")+" — toggle</button></form>";
    page += "<p>Selected: "+String(gd::eventName(gd::SportEvent(effectSelection)))+String(effectMask&(1<<effectSelection)?" ON":" OFF")+"</p>";
    page += "<form method='post' action='/effects'><button name='action' value='1'>Next event</button><button name='action' value='2'>Toggle event</button><button name='action' value='3'>Preview away</button><button name='action' value='4'>Preview home</button></form>";
    page+="</section>";
    }
    page.replace("</form>","<input type='hidden' name='return' value='"+current+"'></form>");
    page+="<footer>Local control · GlowDeck v0.8 · <a href='/?page=setup'>Connection and preferences</a></footer></body></html>";
    web.send(200, "text/html; charset=utf-8", page);
}
void refreshNhl() {
    if (WiFi.status() != WL_CONNECTED) { banner("No Wi-Fi for NHL"); return; }
    // Requests are serialized on loopTask; keep bulky scratch data off its stack.
    static gd::Scoreboard incoming;
    String error;
    bool ok = nhl.poll(incoming, error,scheduleDay);
    if(scheduleDay==0){todayOnline=ok;if(ok)todayScores=incoming;}else{static gd::Scoreboard daily;String dailyError;todayOnline=nhl.poll(daily,dailyError,0);if(todayOnline)todayScores=daily;}
    lastNhl = millis();
    nhlEverPolled = true;
    Serial.printf("NHL memory: stack minimum free=%u bytes, heap free=%u bytes\n",
        unsigned(uxTaskGetStackHighWaterMark(nullptr)),unsigned(ESP.getFreeHeap()));
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
    const gd::Game *fresh=gd::findGame(scores,uiState.selectedGameId);
    if(fresh)trackedGame=*fresh;
    const gd::Game *tracked = selectedGame();
    if (hadPrevious && tracked) {
        gd::GoalChange change = gd::detectGoalChange(previous, *tracked);
        if (change.changed) banner(String("SCORE! Away +") + change.awayAdded + " Home +" + change.homeAdded);
    }
    bool wasArmed=goals.armed();
    goals.observeGame(tracked);
    if(wasArmed&&!goals.armed()){saveResume();banner("Game ended or cancelled; monitoring stopped");}
    // A selected game may be absent from this browsing day; keep tracking its ID.
    if (uiState.gamePage * 4 >= scores.count) uiState.gamePage = 0;
    Serial.printf("NHL: %u games for %s\n", scores.count, scores.date);
    redraw();
}
void refreshPlayByPlay() {
    if (!goals.armed() || WiFi.status()!=WL_CONNECTED) return;
    static gd::GoalFeed sample; String error;
    const bool wasPaused=goals.paused();
    const bool ok=nhl.pollGoals(goals.gameId(),sample,error,&trackedGame);
    lastPbp=millis(); pbpEverPolled=true;
    if (!ok) {
        goals.pause();
        banner(String("NHL PBP: ")+error);
        return;
    }
    bool wasArmed=goals.armed();
    goals.observeFeed(sample,millis());
    if(wasArmed&&!goals.armed()){saveResume();banner("Game ended or cancelled; monitoring stopped");}
    if (wasPaused && goals.baselined()) banner(goals.waiting()?"ARMED - waiting for game start":"NHL play baseline ready");
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
      case ActionType::ToggleResume:
        resumeEnabled=!resumeEnabled;if(!resumeEnabled)resumePending=false;saveResume();banner(resumeEnabled?"Reboot resume enabled":"Reboot resume disabled");break;
      case ActionType::ScheduleDay:
        scheduleDay=(scheduleDay+1)%7;uiState.gamePage=0;refreshNhl();break;
      case ActionType::PreviousLeds: uiState.ledPage=max(0,uiState.ledPage-1);break;
      case ActionType::NextLeds: uiState.ledPage=min(max(0,(int(wled.size())-1)/3),uiState.ledPage+1);break;
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
        banner(wled.scanOverflow?"8-device limit; remove unused device":String("Found ") + wled.size() + " WLED device(s)"); break;
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
      case ActionType::ShowDisplay: ui.setPage(Page::Display);break;
      case ActionType::FollowAdd: followSelected();break;
      case ActionType::FollowStart: startFollowing();break;
      case ActionType::FollowStop: following.stop();audio.stop();break;
      case ActionType::EffectsToggle: effectAction(0);break;
      case ActionType::EffectsNext: effectAction(1);break;
      case ActionType::EffectsEnable: effectAction(2);break;
      case ActionType::EffectsAway: effectAction(3);break;
      case ActionType::EffectsHome: effectAction(3,true);break;
      case ActionType::AudioPreviewAway: previewSound(false); break;
      case ActionType::AudioPreviewHome: previewSound(true); break;
      default: break;
    }
    redraw();
}
void setupWeb() {
    if (servicesStarted) return;
    web.on("/", HTTP_GET, webHome);
    web.on("/follow",HTTP_POST,[]{
        String op=web.arg("op");
        if(op=="stop"){following.stop();audio.stop();banner("Following stopped; restoring");}
        else if(op=="start")startFollowing();
        else if(following.busy())banner("Stop / restore before editing followed games");
        else if(op=="add")followSelected();
        else if(op=="targets"){following.targetIp[0]=web.arg("t0");following.targetIp[1]=web.arg("t1");if(!following.save())banner("Save failed");}
        else {int i=web.arg("i").toInt();if(i>=0&&i<int(following.count)){
            if(op=="remove"){for(unsigned n=i+1;n<following.count;++n)following.games[n-1]=following.games[n];--following.count;}
            else if(op=="save"){auto &f=following.games[i];f.display=web.hasArg("display");f.sound=web.hasArg("sound");f.targets=(web.hasArg("a")?1:0)|(web.hasArg("b")?2:0);f.ddp=web.hasArg("ddp");f.mode=gd::validatedMode(web.arg("mode").toInt());f.delay=constrain(web.arg("delay").toInt(),0,90);f.mask=0;for(int e=0;e<7;++e)if(web.hasArg("e"+String(e)))f.mask|=1<<e;}
            if(!following.save())banner("Save failed");
        }}redraw();redirectHome();
    });
    web.on("/schedule",HTTP_POST,[]{scheduleDay=constrain(web.arg("day").toInt(),0,6);uiState.gamePage=0;refreshNhl();redirectHome();});
    web.on("/resume",HTTP_POST,[]{resumeEnabled=!resumeEnabled;if(!resumeEnabled)resumePending=false;saveResume();redirectHome();});
    web.on("/timeformat",HTTP_POST,[]{
        const String format=web.arg("format");
        if(format!="12"&&format!="24"){web.send(400,"text/plain","Choose 12 or 24");return;}
        Preferences p;
        if(!p.begin("gd-resume",false)){web.send(500,"text/plain","Unable to save setting");return;}
        const bool saved=p.putBool("hour24",format=="24")==1;p.end();
        if(!saved){web.send(500,"text/plain","Unable to save setting");return;}
        hour24=format=="24";banner(hour24?"Time format: 24-hour":"Time format: 12-hour");redraw();redirectHome();
    });
    web.on("/timezone",HTTP_POST,[]{
        String zone=web.arg("tz");
        if(zone=="UTC0"||zone=="MST7MDT,M3.2.0,M11.1.0"||zone=="PST8PDT,M3.2.0,M11.1.0"||zone=="CST6CDT,M3.2.0,M11.1.0"||zone=="EST5EDT,M3.2.0,M11.1.0") {
            Preferences p;if(p.begin("gd-resume",false)){p.putString("tz",zone);p.end();}setenv("TZ",zone.c_str(),1);tzset();
            banner("Timezone saved; reopen scores");
        }redirectHome();
    });
    web.on("/selectled",HTTP_POST,[]{int i=web.arg("index").toInt();if(i>=0&&i<int(wled.size())&&clearAutoBeforeManual()){uiState.selectedLed=i;uiState.ledPage=i/3;}redirectHome();});
    web.on("/effects",HTTP_POST,[]{
        String a=web.arg("action");
        if(a=="0"||a=="1"||a=="2"||a=="3")effectAction(a.toInt());
        else if(a=="4")effectAction(3,true);
        redirectHome();
    });
    web.on("/add", HTTP_POST, [] {
        String err;
        if (wled.add(web.arg("ip"), err)) {
            if (uiState.selectedLed < 0) uiState.selectedLed = 0;
            banner("WLED added");
        } else banner(err);
        redirectHome();
    });
    web.on("/remove", HTTP_POST, [] {
        if(clearAutoBeforeManual()) {
            String selectedIp=uiState.selectedLed>=0&&size_t(uiState.selectedLed)<wled.size()?wled.at(uiState.selectedLed).ip:String("");
            if(wled.remove(web.arg("ip"))) {
                uiState.selectedLed=-1;for(size_t i=0;i<wled.size();++i)if(wled.at(i).ip==selectedIp)uiState.selectedLed=i;
                uiState.ledPage=uiState.selectedLed>=0?uiState.selectedLed/3:0;banner("Device removed");
            }
        }
        redirectHome();
    });
    web.on("/scan", HTTP_POST, [] { wled.discover(); if (wled.size() && uiState.selectedLed<0) uiState.selectedLed=0; banner(wled.scanOverflow?"8-device limit; remove unused device":"Discovery complete"); redirectHome(); });
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
// Serial C at boot forces recalibration even if the existing map cannot hit UI controls.
void calibrateTouch(bool force=false) {
    Preferences prefs;
    if (!prefs.begin("touchmap",false)) return;
    const char *key = (GD_DISPLAY_ROTATION % 2) == 0 ? "landscape" : "portrait";
    if (!force && prefs.getBytesLength(key)==sizeof(touchMap)) {
        prefs.getBytes(key,&touchMap,sizeof(touchMap)); prefs.end(); return;
    }
    int x[3],y[3]; bool ok=true;
    for (int i=0;i<3 && ok;++i) {
        ui.calibrationTarget(i);
        uint32_t start=millis();
        while(touch.touched() && millis()-start<20000) delay(10);
        delay(150);
        while(!touch.touched() && millis()-start<20000) delay(10);
        if(millis()-start>=20000) { ok=false; break; }
        long sx=0,sy=0; int n=0;
        for(int j=0;j<8;++j) {
            if(touch.touched()) { TS_Point p=touch.getPoint();sx+=p.x;sy+=p.y;++n; }
            delay(15);
        }
        if(n<3) {ok=false;break;}
        x[i]=sx/n;y[i]=sy/n;
    }
    gd::TouchTransform candidate;
    if(ok && candidate.fit(x,y)) {
        touchMap=candidate;prefs.putBytes(key,&touchMap,sizeof(touchMap));
        Serial.println("Touch calibration saved");
    } else Serial.println("Touch calibration cancelled; retaining previous mapping");
    prefs.end();
    lastTap=millis();
}
void pollTouch() {
    if (touchSuspendedForSd) return;
    if (!touch.touched() || millis() - lastTap < 230) return;
    TS_Point p = touch.getPoint();
    // XPT2046 setRotation(1) reports a rotated raw space for landscape.
    int x = touchMap.screenX(p.x,p.y);
    int y = touchMap.screenY(p.x,p.y);
    Serial.printf("Touch raw(%d,%d) screen(%d,%d)\n", p.x, p.y, x, y);
    lastTap = millis();
    doAction(ui.touch(x, y, uiState));
}
}

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.printf("GlowDeck ESP32 v0.8.1 - loop stack %u bytes\n",unsigned(getArduinoLoopTaskStackSize()));
    uiState.following=&following;
    uiState.scoreboard = &scores;
    uiState.leds = &wled;
    ui.begin();
    touchSpi.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin(touchSpi);
    touch.setRotation(1);
    Serial.println("Send C within 2 seconds to calibrate touch");
    delay(2000);
    bool force=false;
    while(Serial.available()) { int c=Serial.read();if(c=='c'||c=='C')force=true; }
    if(force) calibrateTouch(true);
    else {
        Preferences prefs;
        if(prefs.begin("touchmap",false)) {
            const char *key = (GD_DISPLAY_ROTATION % 2) == 0 ? "landscape" : "portrait";
            if(prefs.getBytesLength(key)==sizeof(touchMap)) prefs.getBytes(key,&touchMap,sizeof(touchMap));
            prefs.end();
        }
    }
    wled.begin();following.begin();
    if(!audio.begin())Serial.println("Audio hardware setup: "+audio.lastError());
    configTime(0,0,"pool.ntp.org","time.nist.gov");
    loadGameSettings();
    if (wled.size()) uiState.selectedLed = 0;
    redraw();
    initWiFi();
    if (WiFi.status() == WL_CONNECTED) refreshNhl();
    if(resumePending)banner("Saved monitor: checking game and WLED");
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
    following.tick(nhl,online);
    if(!audio.busy()) {gd::QueuedReaction event;if(following.takeSound(event)&&soundEnabled){
        const auto &g=following.games[event.game].game;String code=event.home?g.home:g.away;
        gd::AudioChoice choice;Preferences p;if(p.begin("gd-sound",true)){
            choice.kind=gd::validatedSound(p.getUChar(("k"+code).c_str(),1));
            String clip=p.getString(("f"+code).c_str(),"GOAL");if(gd::validClipName(clip.c_str()))snprintf(choice.clip,sizeof(choice.clip),"%s",clip.c_str());p.end();}
        trySound(choice);
    }}
    if(resumePending && online && !following.busy()) {
        static uint32_t lastResume=0;
        if(!lastResume || millis()-lastResume>30000) {
            lastResume=millis(); static gd::GoalFeed feed;String err;
            if(nhl.pollGoals(trackedGame.id,feed,err,&trackedGame)) {
                if(!strcmp(trackedGame.state,"OFF")||!strcmp(trackedGame.state,"FINAL")||!strcmp(trackedGame.state,"CNCL")){resumePending=false;saveResume();banner("Saved game ended or cancelled");}
                else if(gd::isLive(trackedGame)||gd::isUpcoming(trackedGame)) {
                    for(size_t i=0;i<wled.size();++i)if(wled.at(i).ip==resumeIp)uiState.selectedLed=i;
                    if(uiState.selectedLed>=0 && wled.at(uiState.selectedLed).ip==resumeIp && !wled.autoRestorePending() && wled.probe(uiState.selectedLed)) {
                        if(goals.arm(&trackedGame,millis())) {
                            goals.observeFeed(feed,millis());reactions.setTeamColors(gd::teamColor(trackedGame.away),gd::teamColor(trackedGame.home));
                            resumePending=false;saveResume();banner("Monitoring resumed; old events skipped");
                        }
                    }
                }
            }
        }
    }
    const uint32_t started = reactions.startedCount(), restored = reactions.restoreCount();
    if(effectPreview && (!online || (int32_t)(millis()-effectPreviewEnds)>=0)){
        effectPreview=false;String error;if(!wled.restoreAuto(error))banner(error);
    }
    bool wasArmed=goals.armed();
    if(!effectPreview)reactions.service(millis(), online);
    if(wasArmed&&!goals.armed())saveResume();
    // Recovery must also work if NHL is unavailable or no WLED is selected.
    // See implementation: the driver should only depend on Wi-Fi for restore.
    if (reactions.startedCount() != started) {
        banner(String(gd::eventName(reactions.lastEventKind()))+" effect active");
        if(soundEnabled && reactions.lastEventKind()==gd::SportEvent::Goal)trySound(reactions.lastGoalHome()?homeSound:awaySound);
    }
    if(reactions.restoreCount()!=restored)audio.stop();
    if (reactions.restoreCount() != restored) banner("Original lights restored");
    if (!uiState.message.isEmpty() && millis() - bannerSet > 6000) uiState.message = "";
    if (millis() - lastUi > UI_INTERVAL_MS) redraw();
    delay(12);
}
