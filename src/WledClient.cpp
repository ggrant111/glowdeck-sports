#include "WledClient.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

namespace {
bool parseIp(const String &value, IPAddress &out) {
    if (value.length() > 15 || !out.fromString(value)) return false;
    // Refuse multicast, wildcard and loopback addresses.
    if (out[0] == 0 || out[0] == 127 || out[0] >= 224) return false;
    return true;
}
bool request(const String &ip, const char *path, const String *post, String &reply) {
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(2000);
    if (!http.begin(client, String("http://") + ip + path)) return false;
    int code = -1;
    if (post) { http.addHeader("Content-Type", "application/json"); code = http.POST(*post); }
    else code = http.GET();
    bool ok = code == HTTP_CODE_OK;
    if (ok && !post) reply = http.getString();
    http.end();
    return ok;
}
}

void WledClient::begin() { restore(); }
void WledClient::restore() {
    Preferences p;
    p.begin("gd-leds", true);
    String list = p.getString("ips", "");
    p.end();
    int start = 0;
    while (start < (int)list.length() && count_ < gd::MAX_LEDS) {
        int end = list.indexOf(',', start);
        if (end < 0) end = list.length();
        String ip = list.substring(start, end);
        IPAddress address;
        if (parseIp(ip, address)) { devices_[count_].ip = ip; devices_[count_].name = ip; ++count_; }
        start = end + 1;
    }
}
void WledClient::persist() {
    String list;
    for (size_t i=0; i<count_; ++i) {
        if (i) list += ',';
        list += devices_[i].ip;
    }
    Preferences p;
    p.begin("gd-leds", false);
    p.putString("ips", list);
    p.end();
}
bool WledClient::add(const String &host, String &error) {
    String ip = host;
    ip.trim();
    IPAddress address;
    if (!parseIp(ip, address)) { error = "Enter a numeric IPv4 address"; return false; }
    for (size_t i=0; i<count_; ++i) if (devices_[i].ip == ip) return true;
    if (count_ >= gd::MAX_LEDS) { error = "WLED list full (8)"; return false; }
    String reply;
    if (!request(ip, "/json/info", nullptr, reply)) { error = "Could not reach WLED at " + ip; return false; }
    DynamicJsonDocument doc(2048);
    if (deserializeJson(doc, reply) || !doc["ver"].is<const char*>()) {
        error = "Device did not identify as WLED";
        return false;
    }
    LedDevice &d = devices_[count_++];
    d.ip = ip;
    d.name = doc["name"].as<String>();
    if (d.name.isEmpty()) d.name = ip;
    d.online = true;
    persist();
    return true;
}
void WledClient::discover() {
    if (WiFi.status() != WL_CONNECTED) return;
    int n = MDNS.queryService("wled", "tcp");
    for (int i=0; i<n; ++i) {
        String err;
        IPAddress addr = MDNS.IP(i);
        if ((uint32_t)addr != 0) add(addr.toString(), err);
    }
}
bool WledClient::remove(const String &ip) {
    for (size_t i=0; i<count_; ++i) if (devices_[i].ip == ip) {
        for (size_t j=i+1; j<count_; ++j) devices_[j-1] = devices_[j];
        devices_[--count_] = LedDevice();
        persist();
        return true;
    }
    return false;
}
bool WledClient::probe(size_t index) {
    if (index >= count_) return false;
    String reply;
    bool ok = request(devices_[index].ip, "/json/info", nullptr, reply);
    devices_[index].online = ok;
    return ok;
}
bool WledClient::applyColor(size_t index, uint32_t rgb, String &error) {
    if (index >= count_) { error = "Choose a WLED device"; return false; }
    // WLED's default segment, id 0. Sprint 2 will support multiple segments/zones.
    DynamicJsonDocument doc(256);
    doc["on"] = true;
    doc["bri"] = 160;
    JsonObject seg = doc.createNestedArray("seg").createNestedObject();
    seg["id"] = 0;
    seg["fx"] = 0;
    JsonArray col = seg.createNestedArray("col").createNestedArray();
    col.add((rgb >> 16) & 0xff);
    col.add((rgb >> 8) & 0xff);
    col.add(rgb & 0xff);
    String body;
    serializeJson(doc, body);
    String reply;
    bool ok = request(devices_[index].ip, "/json/state", &body, reply);
    devices_[index].online = ok;
    if (!ok) error = "WLED color request failed";
    return ok;
}
bool WledClient::setPower(size_t index, bool on, String &error) {
    if (index >= count_) { error = "Choose a WLED device"; return false; }
    String body = on ? "{\"on\":true}" : "{\"on\":false}";
    String reply;
    bool ok = request(devices_[index].ip, "/json/state", &body, reply);
    devices_[index].online = ok;
    if (!ok) error = "WLED power request failed";
    return ok;
}
