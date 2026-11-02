#pragma once
#include <Arduino.h>
#include "GameCore.h"
#include "DdpStream.h"

struct LedDevice {
    String ip;
    String name;
    bool online = false;
};

class WledClient {
public:
    void begin();
    void discover();
    bool scanOverflow=false;
    bool add(const String &host, String &error);
    bool remove(const String &ip);
    bool applyColor(size_t index, uint32_t rgb, String &error);
    bool setPower(size_t index, bool on, String &error);
    bool probe(size_t index);
    // Automatic transactions are isolated to one target; a recovery journal
    // survives board reboots and must be restored BEFORE the next transaction.
    bool beginAutoColor(size_t index, uint32_t rgb, String &error);
    bool restoreAuto(String &error);
    bool beginAutoEvent(size_t index,uint32_t rgb,gd::SportEvent kind,String &error);
    bool ddpEnabled=false;
    bool autoRestorePending() const { return !autoBackup_.isEmpty(); }
    size_t size() const { return count_; }
    const LedDevice& at(size_t i) const { return devices_[i]; }
private:
    DdpStream stream_;
    gd::SportEvent effect_=gd::SportEvent::Goal;
    LedDevice devices_[gd::MAX_LEDS];
    size_t count_ = 0;
    String autoBackup_; // JSON object: {"ip":"...","state":{...}}
    void restore();
    void persist();
    void loadAutoBackup();
    bool persistAutoBackup(const String &value);
};
