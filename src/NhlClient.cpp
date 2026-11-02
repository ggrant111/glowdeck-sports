#include "NhlClient.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string.h>

namespace {
void copy(char* dest, size_t len, const char *src) {
    if (!src) src = "";
    snprintf(dest, len, "%s", src);
}
}

bool NhlClient::poll(gd::Scoreboard &out, String &error) {
    error = "";
    WiFiClientSecure client;
    // Prototype only: allow encrypted transport without certificate validation.
    // Before production, replace with a verified CA store and validate TLS hostname.
    client.setInsecure();
    client.setTimeout(10000);
    HTTPClient http;
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, "https://api-web.nhle.com/v1/score/now")) {
        error = "NHL connection setup failed";
        return false;
    }
    http.addHeader("Accept", "application/json");
    const int status = http.GET();
    if (status != HTTP_CODE_OK) {
        error = "NHL HTTP " + String(status);
        http.end();
        return false;
    }
    // Filter + stream parsing dramatically reduces memory use vs a full JSON response.
    StaticJsonDocument<768> filter;
    filter["currentDate"] = true;
    // ArduinoJson array filter index 0 is applied to every game in the array.
    filter["games"][0]["id"] = true;
    filter["games"][0]["gameState"] = true;
    filter["games"][0]["awayTeam"]["abbrev"] = true;
    filter["games"][0]["awayTeam"]["score"] = true;
    filter["games"][0]["homeTeam"]["abbrev"] = true;
    filter["games"][0]["homeTeam"]["score"] = true;
    filter["games"][0]["period"] = true;
    filter["games"][0]["clock"]["timeRemaining"] = true;
    // 16 games x small filtered entries, 16 KiB sufficient for ordinary NHL slate.
    DynamicJsonDocument doc(16384);
    DeserializationError result = deserializeJson(doc, http.getStream(),
        DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(12));
    http.end();
    if (result) {
        error = String("NHL JSON: ") + result.c_str();
        return false;
    }
    if (!doc["games"].is<JsonArray>()) {
        error = "NHL response missing games";
        return false;
    }
    gd::Scoreboard temp;
    copy(temp.date, sizeof(temp.date), doc["currentDate"] | "");
    for (JsonObject item : doc["games"].as<JsonArray>()) {
        if (temp.count >= gd::MAX_GAMES) break;
        gd::Game &game = temp.games[temp.count];
        game.id = item["id"] | 0UL;
        if (!game.id) continue;
        copy(game.away, sizeof(game.away), item["awayTeam"]["abbrev"] | "?");
        copy(game.home, sizeof(game.home), item["homeTeam"]["abbrev"] | "?");
        copy(game.state, sizeof(game.state), item["gameState"] | "");
        copy(game.clock, sizeof(game.clock), item["clock"]["timeRemaining"] | "");
        game.awayScore = (uint8_t)(item["awayTeam"]["score"] | 0);
        game.homeScore = (uint8_t)(item["homeTeam"]["score"] | 0);
        game.period = (uint8_t)(item["period"] | 0);
        ++temp.count;
    }
    temp.valid = true;
    out = temp;
    return true;
}

bool NhlClient::pollGoals(uint32_t gameId, gd::GoalFeed &out, String &error) {
    error = "";
    if (gameId < 2000000000UL || gameId > 2999999999UL) {
        error="Invalid NHL game ID"; return false;
    }
    WiFiClientSecure client;
    // Prototype TLS limitation: see ROADMAP and release blockers.
    client.setInsecure();
    client.setTimeout(10000);
    HTTPClient http;
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    const String url="https://api-web.nhle.com/v1/gamecenter/"+String(gameId)+"/play-by-play";
    if (!http.begin(client,url)) { error="PBP connection failed"; return false; }
    http.addHeader("Accept","application/json");
    const int status=http.GET();
    if (status!=HTTP_CODE_OK) { error="PBP HTTP "+String(status); http.end(); return false; }

    // Filtered streaming read avoids holding enormous full play objects in RAM.
    StaticJsonDocument<1024> filter;
    filter["id"]=true;
    filter["gameState"]=true;
    filter["plays"][0]["periodDescriptor"]["periodType"]=true;
    filter["plays"][0]["situationCode"]=true;
    filter["awayTeam"]["id"]=true;
    filter["homeTeam"]["id"]=true;
    filter["plays"][0]["eventId"]=true;
    filter["plays"][0]["typeDescKey"]=true;
    filter["plays"][0]["details"]["eventOwnerTeamId"]=true;
    DynamicJsonDocument doc(72*1024);
    const DeserializationError parse=deserializeJson(doc,http.getStream(),
        DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(14));
    http.end();
    if(parse) { error=String("PBP JSON: ")+parse.c_str(); return false; }
    if(!doc["plays"].is<JsonArray>() || !doc["awayTeam"]["id"].is<int>() ||
       !doc["homeTeam"]["id"].is<int>()) {
        error="PBP missing required fields"; return false;
    }
    gd::GoalFeed sample;
    sample.gameId=doc["id"].as<uint32_t>();
    sample.awayId=doc["awayTeam"]["id"].as<uint32_t>();
    sample.homeId=doc["homeTeam"]["id"].as<uint32_t>();
    if(sample.gameId!=gameId || !sample.awayId || !sample.homeId ||
       sample.awayId==sample.homeId) {
        error="PBP game/team mismatch"; return false;
    }
    const char *state=doc["gameState"]|"";
    sample.final=strcmp(state,"OFF")==0||strcmp(state,"FINAL")==0;
    int previousAdvantage=-1;
    for(JsonObject play: doc["plays"].as<JsonArray>()) {
        const uint32_t id=play["eventId"] | 0UL;
        if(!id) continue;
        if(id>sample.latestEventId)sample.latestEventId=id;
        const char *type=play["typeDescKey"] | "";
        gd::ExtraPlay cue;cue.id=id;bool found=false;
        if(strcmp(type,"period-start")==0){cue.kind=gd::periodStart(play["periodDescriptor"]["periodType"]|"");found=true;}
        else if(strcmp(type,"period-end")==0){cue.kind=gd::SportEvent::PeriodEnd;found=true;}
        else if(strcmp(type,"game-end")==0){cue.kind=gd::SportEvent::GameEnd;found=true;}
        int advantage=gd::advantage(play["situationCode"]|"");
        // A burst at the first play confirming an advantage, not on every penalty.
        if(!found && advantage>0 && previousAdvantage>=0 && advantage!=previousAdvantage){
            cue.kind=gd::SportEvent::PowerPlay;cue.ownerId=advantage==2?sample.homeId:sample.awayId;found=true;
        }
        if(advantage>=0)previousAdvantage=advantage;
        if(found){if(sample.extraCount>=64){error="Too many event cues";return false;}sample.extra[sample.extraCount++]=cue;}
        if(strcmp(type,"goal")!=0) continue;
        if(strcmp(play["periodDescriptor"]["periodType"]|"","SO")==0)continue;
        if(sample.count>=gd::MAX_GOALS_PER_GAME) {
            error="Too many goals in play feed"; return false;
        }
        gd::GoalPlay &goal=sample.goals[sample.count++];
        goal.id=id;
        goal.ownerId=play["details"]["eventOwnerTeamId"] | 0UL;
    }
    sample.valid=true;
    out=sample;
    return true;
}
