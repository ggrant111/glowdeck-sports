#pragma once
#include <Arduino.h>
#include "AudioModel.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
class AudioPlayer {
public:
    bool begin();
    void stop();
    bool play(gd::AudioChoice choice, uint8_t volume);
    bool busy() const {return busy_;}
    bool needsTouchFree() const {return sdSession_;}
    String lastError() const {return error_;}
    void finishTouchReturn() {sdSession_=false;}
private:
    static void taskThunk(void* arg);
    void taskLoop();
    void run(gd::AudioChoice choice, uint8_t vol);
    void preset(gd::SoundKind kind,uint8_t vol);
    bool custom(const char *clip,uint8_t vol);
    volatile bool busy_=false, cancel_=false, sdSession_=false;
    TaskHandle_t task_=nullptr;
    gd::AudioChoice request_;
    uint8_t requestVolume_=50;
    String error_;
};
