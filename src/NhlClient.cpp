#include "NhlClient.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <string.h>
#include <time.h>

namespace {
void copy(char* dest, size_t len, const char *src) {
    if (!src) src = "";
    snprintf(dest, len, "%s", src);
}
}

bool NhlClient::poll(gd::Scoreboard &out, String &error, uint8_t dayOffset) {
    error = "";
    WiFiClientSecure client;
    // Prototype only: allow encrypted transport without certificate validation.
    // Before production, replace with a verified CA store and validate TLS hostname.
    client.setInsecure();
    client.setTimeout(10000);
    HTTPClient http;
    // getStream() bypasses HTTPClient chunk decoding. Request an unchunked body.
    http.useHTTP10(true);
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    String date="now";
    time_t now=time(nullptr);
    if(dayOffset && now<1700000000) {error="Clock syncing; retry schedule";return false;}
    if(now>=1700000000) {
        struct tm day;localtime_r(&now,&day);day.tm_mday+=min(dayOffset,uint8_t(6));
        day.tm_hour=12;day.tm_min=0;day.tm_sec=0;day.tm_isdst=-1;mktime(&day);
        char stamp[11];strftime(stamp,sizeof(stamp),"%Y-%m-%d",&day);date=stamp;
    }
    if (!http.begin(client, "https://api-web.nhle.com/v1/score/"+date)) {
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
    StaticJsonDocument<1536> filter;
    filter["currentDate"] = true;
    filter["games"][0]["startTimeUTC"] = true;
    filter["games"][0]["gameScheduleState"] = true;
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
    if (filter.overflowed()) { error="NHL filter overflow"; http.end(); return false; }
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
        const char *schedule=item["gameScheduleState"]|"";
        if(!strcmp(schedule,"PPD")||!strcmp(schedule,"CNCL"))copy(game.state,sizeof(game.state),schedule);
        copy(game.startTime,sizeof(game.startTime),item["startTimeUTC"]|"");
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

bool NhlClient::pollGoals(uint32_t gameId, gd::GoalFeed &out, String &error, gd::Game *details) {
    error = "";
    if (gameId < 2000000000UL || gameId > 2999999999UL) {
        error="Invalid NHL game ID"; return false;
    }
    WiFiClientSecure client;
    // Prototype TLS limitation: see ROADMAP and release blockers.
    client.setInsecure();
    client.setTimeout(10000);
    HTTPClient http;
    // getStream() bypasses HTTPClient chunk decoding. Request an unchunked body.
    http.useHTTP10(true);
    http.setTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    const String url="https://api-web.nhle.com/v1/gamecenter/"+String(gameId)+"/play-by-play";
    if (!http.begin(client,url)) { error="PBP connection failed"; return false; }
    http.addHeader("Accept","application/json");
    const int status=http.GET();
    if (status!=HTTP_CODE_OK) { error="PBP HTTP "+String(status); http.end(); return false; }

    // Filtered streaming read avoids holding enormous full play objects in RAM.
    StaticJsonDocument<2048> filter;
    filter["startTimeUTC"]=true; filter["gameScheduleState"]=true;
    filter["awayTeam"]["abbrev"]=true;filter["homeTeam"]["abbrev"]=true;
    filter["awayTeam"]["score"]=true;filter["homeTeam"]["score"]=true;
    filter["periodDescriptor"]["number"]=true;filter["clock"]["timeRemaining"]=true;
    filter["id"]=true;
    filter["gameState"]=true;
    filter["plays"][0]["periodDescriptor"]["periodType"]=true;
    filter["plays"][0]["situationCode"]=true;
    filter["awayTeam"]["id"]=true;
    filter["homeTeam"]["id"]=true;
    filter["plays"][0]["eventId"]=true;
    filter["plays"][0]["typeDescKey"]=true;
    filter["plays"][0]["details"]["eventOwnerTeamId"]=true;
    if (filter.overflowed()) { error="PBP filter overflow"; http.end(); return false; }
    DynamicJsonDocument doc(72*1024);
    const DeserializationError parse=deserializeJson(doc,http.getStream(),
        DeserializationOption::Filter(filter),DeserializationOption::NestingLimit(14));
    http.end();
    if(parse) { error=String("PBP JSON: ")+parse.c_str(); return false; }
    const char *feedState=doc["gameState"]|"";
    const char *scheduleState=doc["gameScheduleState"]|"";
    if(!strcmp(scheduleState,"PPD")||!strcmp(scheduleState,"CNCL"))feedState=scheduleState;
    const bool pre=!strcmp(feedState,"FUT")||!strcmp(feedState,"PRE")||!strcmp(feedState,"PPD")||!strcmp(feedState,"CNCL");
    if((!pre && !doc["plays"].is<JsonArray>()) || !doc["awayTeam"]["id"].is<int>() ||
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
    if(details) {
        details->id=sample.gameId;
        copy(details->away,sizeof(details->away),doc["awayTeam"]["abbrev"]|"?");
        copy(details->home,sizeof(details->home),doc["homeTeam"]["abbrev"]|"?");
        copy(details->state,sizeof(details->state),feedState);
        copy(details->startTime,sizeof(details->startTime),doc["startTimeUTC"]|"");
        copy(details->clock,sizeof(details->clock),doc["clock"]["timeRemaining"]|"");
        details->awayScore=doc["awayTeam"]["score"]|0;details->homeScore=doc["homeTeam"]["score"]|0;
        details->period=doc["periodDescriptor"]["number"]|0;
    }
    const char *state=feedState;
    copy(sample.state,sizeof(sample.state),state);
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
