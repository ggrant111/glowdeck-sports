#include "GoalEngine.h"

namespace gd {
namespace {
bool reached(uint32_t now, uint32_t due) { return (int32_t)(now - due) >= 0; }
bool accepted(bool home, CelebrationMode mode) {
    return mode == CelebrationMode::Both ||
        (mode == CelebrationMode::Home && home) ||
        (mode == CelebrationMode::Away && !home);
}
}
void GoalEngine::selectGame(uint32_t id) {
    disarm(); gameId_ = id; cooldown_ = false; lastEventId_ = 0;
}
void GoalEngine::setMode(CelebrationMode mode) {
    mode_ = validatedMode(static_cast<uint8_t>(mode));
    cancelPending();
}
void GoalEngine::setDelay(uint8_t seconds) {
    delaySeconds_ = seconds > 90 ? 90 : seconds;
    cancelPending();
}
bool GoalEngine::arm(const Game *current, uint32_t nowMs) {
    if (!current || !gameId_ || current->id != gameId_ || !isLive(*current)) return false;
    pending_ = GoalEvent();
    armed_ = true; paused_ = true; baselineReady_ = false;
    lastEventId_ = 0; homeId_ = 0; awayId_ = 0;
    lastFresh_ = nowMs; cooldown_ = false;
    return true;
}
void GoalEngine::disarm() {
    armed_ = false; paused_ = false; baselineReady_ = false;
    cancelPending();
}
void GoalEngine::pause() {
    if (!armed_) return;
    paused_ = true; baselineReady_ = false; cancelPending();
}
void GoalEngine::observeGame(const Game *current) {
    if (armed_ && (!current || current->id != gameId_ || !isLive(*current))) disarm();
}
void GoalEngine::observeFeed(const GoalFeed &feed, uint32_t nowMs) {
    if (!armed_) return;
    if (!feed.valid || feed.gameId != gameId_ || !feed.homeId || !feed.awayId ||
        feed.homeId == feed.awayId || feed.count > MAX_GOALS_PER_GAME) {
        pause(); return;
    }
    if (paused_ || !baselineReady_ || feed.latestEventId < lastEventId_ ||
        homeId_ != feed.homeId || awayId_ != feed.awayId) {
        lastEventId_ = feed.latestEventId;
        homeId_ = feed.homeId; awayId_ = feed.awayId;
        lastFresh_ = nowMs; paused_ = false; baselineReady_ = true;
        return; // first successful sample is strictly a baseline
    }
    uint8_t goals = 0;
    GoalEvent candidate;
    for (uint8_t i=0; i<feed.count; ++i) {
        const GoalPlay &p = feed.goals[i];
        if (p.id <= lastEventId_) continue;
        ++goals;
        if (p.ownerId == feed.awayId || p.ownerId == feed.homeId) {
            candidate.valid = true;
            candidate.home = p.ownerId == feed.homeId;
            candidate.eventId = p.id;
            candidate.gameId = gameId_;
        }
    }
    lastFresh_ = nowMs;
    // Prevent duplicate and out-of-order events. NHL event IDs are expected
    // to increase; backwards feed resets are rebaselined above.
    if (feed.latestEventId > lastEventId_) lastEventId_ = feed.latestEventId;
    if (goals != 1 || !candidate.valid || !accepted(candidate.home, mode_) ||
        pending_.valid || (cooldown_ && !reached(nowMs, cooldownUntil_))) return;
    pending_ = candidate;
    dueAt_ = nowMs + uint32_t(delaySeconds_) * 1000U;
}
void GoalEngine::expire(uint32_t nowMs) {
    if (armed_ && baselineReady_ && (uint32_t)(nowMs-lastFresh_) > 75000U) pause();
    if (pending_.valid && reached(nowMs, dueAt_) &&
        (uint32_t)(nowMs-dueAt_) > 75000U) cancelPending();
}
bool GoalEngine::ready(uint32_t nowMs) const {
    if (!armed_ || paused_ || !baselineReady_ || !pending_.valid) return false;
    if ((uint32_t)(nowMs-lastFresh_) > 75000U) return false;
    if (reached(nowMs,dueAt_) && (uint32_t)(nowMs-dueAt_) > 75000U) return false;
    return reached(nowMs,dueAt_);
}
GoalEvent GoalEngine::consume(uint32_t nowMs) {
    if (!ready(nowMs)) return GoalEvent();
    GoalEvent e=pending_; pending_=GoalEvent();
    cooldown_=true; cooldownUntil_=nowMs+20000U;
    return e;
}
void ReactionController::service(uint32_t nowMs, bool networkOk) {
    if (!networkOk) engine_.pause();
    engine_.expire(nowMs);
    if (lights_.needsRestore()) {
        if (active_ && reached(nowMs,endAt_)) active_=false;
        if (!active_ || !engine_.armed() || !networkOk) {
            active_=false;
            if (networkOk && (!attemptedRestore_ || (uint32_t)(nowMs-lastRestoreTry_)>=3000U)) {
                attemptedRestore_=true; lastRestoreTry_=nowMs;
                if (lights_.restore()) { ++restoreCount_; attemptedRestore_=false; }
            }
        }
        return;
    }
    active_=false; attemptedRestore_=false;
    if (!networkOk || !engine_.ready(nowMs)) return;
    GoalEvent e=engine_.consume(nowMs);
    if (!e.valid) return;
    if (lights_.startColor(e.home?homeColor_:awayColor_)) {
        active_=true; endAt_=nowMs+8000U; ++startedCount_;
        lastGoalHome_=e.home; lastEventId_=e.eventId;
    }
}
}
