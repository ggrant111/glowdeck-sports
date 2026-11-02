#pragma once
#include <Arduino.h>
#include <WiFiUdp.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "EventEffects.h"
class DdpStream {
public:
    bool start(const String &ip,int count,gd::SportEvent event,uint32_t color);
    void stop();
private:
    static void run(void *self);
    std::atomic<bool> stop_{true};
    SemaphoreHandle_t done_=nullptr;
    IPAddress address_;
    int count_=0;
    gd::SportEvent event_=gd::SportEvent::Goal;
    uint32_t color_=0;
};
