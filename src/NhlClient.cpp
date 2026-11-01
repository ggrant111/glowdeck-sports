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
