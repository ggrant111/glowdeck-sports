#pragma once
#include "GameCore.h"
#include <stdint.h>

namespace gd {
// Scoreboard-based detection is conservative: one-goal increases only.
// The scoreboard feed does NOT expose an authoritative NHL play/event ID.
// v0.3 may replace this input with goal event IDs from play-by-play.
struct GoalEvent {
    bool valid = false;
    bool home = false;
    bool favorite = false;
    uint32_t gameId = 0;
    uint8_t score = 0;
};

class GoalEngine {
public:
    void selectGame(uint32_t id);
    void setFavoriteHome(bool home);
    void setDelay(uint8_t seconds);
    bool arm(const Game *current, uint32_t nowMs);
    void disarm();
    bool armed() const { return armed_; }
    bool favoriteHome() const { return favoriteHome_; }
    uint8_t delay() const { return delaySeconds_; }
    uint32_t gameId() const { return gameId_; }
    bool pending() const { return pending_.valid; }
    // Only call observe on a fresh, *successful* score API response.
    void observe(const Game *current, uint32_t nowMs);
    void expire(uint32_t nowMs);
    bool ready(uint32_t nowMs) const;
    GoalEvent consume(uint32_t nowMs);
    void cancelPending() { pending_ = GoalEvent(); }
private:
    uint32_t gameId_ = 0;
    bool favoriteHome_ = true;
    uint8_t delaySeconds_ = 0;
    bool armed_ = false;
    bool hasBaseline_ = false;
    Game baseline_{};
    GoalEvent pending_{};
    uint32_t dueAt_ = 0;
    uint32_t lastFresh_ = 0;
    uint32_t cooldownUntil_ = 0;
    bool cooldown_ = false;
};

// Hardware-neutral WLED transaction port. The driver must journal the
// snapshot BEFORE sending a color; needsRestore survives controller resets.
class LightPort {
public:
    virtual ~LightPort() {}
    virtual bool startColor(uint32_t rgb) = 0;
    virtual bool restore() = 0;
    virtual bool needsRestore() const = 0;
};

class ReactionController {
public:
    ReactionController(GoalEngine &engine, LightPort &lights)
      : engine_(engine), lights_(lights) {}
    // Event colors: bright but non-flashing static team colors, for 10 seconds.
    void service(uint32_t nowMs, bool networkOk);
    void setTeamColors(uint32_t away, uint32_t home) { awayColor_ = away; homeColor_ = home; }
    bool active() const { return active_; }
    uint32_t startedCount() const { return startedCount_; }
    uint32_t restoreCount() const { return restoreCount_; }
private:
    GoalEngine &engine_;
    LightPort &lights_;
    bool active_ = false;
    bool attemptedRestore_ = false;
    uint32_t endAt_ = 0;
    uint32_t awayColor_ = 0x31837A;
    uint32_t homeColor_ = 0x31837A;
    uint32_t lastRestoreTry_ = 0;
    uint32_t startedCount_ = 0;
    uint32_t restoreCount_ = 0;
};
}
