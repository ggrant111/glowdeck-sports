#pragma once
#include "GameCore.h"
#include <stdint.h>

namespace gd {
struct GoalEvent {
    bool valid = false;
    bool home = false;
    uint32_t gameId = 0, eventId = 0;
};

class GoalEngine {
public:
    void selectGame(uint32_t id);
    void setMode(CelebrationMode mode);
    void setDelay(uint8_t seconds);
    bool arm(const Game *current, uint32_t nowMs);
    void disarm();
    void pause(); // network/PBP failure: keep armed, cancel pending, rebaseline on recovery
    bool armed() const { return armed_; }
    bool paused() const { return paused_; }
    bool baselined() const { return baselineReady_; }
    CelebrationMode mode() const { return mode_; }
    uint8_t delay() const { return delaySeconds_; }
    uint32_t gameId() const { return gameId_; }
    bool pending() const { return pending_.valid; }
    void observeGame(const Game *current);
    void observeFeed(const GoalFeed &feed, uint32_t nowMs);
    void expire(uint32_t nowMs);
    bool ready(uint32_t nowMs) const;
    GoalEvent consume(uint32_t nowMs);
    void cancelPending() { pending_ = GoalEvent(); }
private:
    uint32_t gameId_ = 0;
    CelebrationMode mode_ = CelebrationMode::Away;
    uint8_t delaySeconds_ = 0;
    bool armed_ = false, paused_ = false, baselineReady_ = false;
    uint32_t lastEventId_ = 0, homeId_ = 0, awayId_ = 0;
    GoalEvent pending_{};
    uint32_t dueAt_ = 0, lastFresh_ = 0, cooldownUntil_ = 0;
    bool cooldown_ = false;
};

class LightPort {
public:
    virtual ~LightPort() {}
    virtual bool startColor(uint32_t rgb) = 0;
    virtual bool restore() = 0;
    virtual bool needsRestore() const = 0;
};
class ReactionController {
public:
    ReactionController(GoalEngine &engine, LightPort &lights) : engine_(engine), lights_(lights) {}
    void service(uint32_t nowMs, bool networkOk);
    void setTeamColors(uint32_t away, uint32_t home) { awayColor_=away; homeColor_=home; }
    bool active() const { return active_; }
    uint32_t startedCount() const { return startedCount_; }
    uint32_t restoreCount() const { return restoreCount_; }
private:
    GoalEngine &engine_;
    LightPort &lights_;
    bool active_ = false, attemptedRestore_ = false;
    uint32_t endAt_ = 0, lastRestoreTry_ = 0;
    uint32_t awayColor_ = 0x31837A, homeColor_ = 0x31837A;
    uint32_t startedCount_ = 0, restoreCount_ = 0;
};
}
