#pragma once
#include <Arduino.h>
#include "GameCore.h"

struct LedDevice {
    String ip;
    String name;
    bool online = false;
};

class WledClient {
public:
    void begin();
    void discover(); // local mDNS discovery; call manually (not every frame)
    bool add(const String &host, String &error); // IP address only (no arbitrary hostname)
    bool remove(const String &ip);
    bool applyColor(size_t index, uint32_t rgb, String &error);
    bool setPower(size_t index, bool on, String &error);
    bool probe(size_t index);
    size_t size() const { return count_; }
    const LedDevice& at(size_t i) const { return devices_[i]; }
private:
    LedDevice devices_[gd::MAX_LEDS];
    size_t count_ = 0;
    void restore();
    void persist();
};
