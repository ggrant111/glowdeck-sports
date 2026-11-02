#include "GoalEngine.h"

namespace gd {
namespace {
bool reached(uint32_t now, uint32_t due) { return (int32_t)(now - due) >= 0; }
}
void GoalEngine::selectGame(uint32_t id) {
    armed_ = false;
    gameId_ = id;
    hasBaseline_ = false;
    pending_ = GoalEvent();
    cooldown_ = false;
}
void GoalEngine::setFavoriteHome(bool home) {
    if (favoriteHome_ != home) { favoriteHome_ = home; pending_ = GoalEvent(); }
}
void GoalEngine::setDelay(uint8_t seconds) {
    if (seconds > 90) seconds = 90;
    if (delaySeconds_ != seconds) { delaySeconds_ = seconds; cancelPending(); }
}
bool GoalEngine::arm(const Game *current, uint32_t nowMs) {
    // Arming requires a valid LIVE game seen in a fresh feed. Don't arm ahead
    // of a game and then mistake a first live score for a new goal.
    if (!current || !gameId_ || current->id != gameId_ || !isLive(*current)) return false;
    baseline_ = *current;
    hasBaseline_ = true;
    lastFresh_ = nowMs;
    pending_ = GoalEvent();
    cooldown_ = false;
    armed_ = true;
    return true;
}
void GoalEngine::disarm() {
    armed_ = false;
    pending_ = GoalEvent();
}
void GoalEngine::observe(const Game *current, uint32_t nowMs) {
    if (!armed_) return;
    if (!current || current->id != gameId_) { disarm(); return; }
    lastFresh_ = nowMs;
    if (!hasBaseline_) { baseline_ = *current; hasBaseline_ = true; return; }
    const Game before = baseline_;
    baseline_ = *current;
    if (!isLive(*current)) { disarm(); return; }
    // Period/game-state transitions and any score correction are never goals.
    if (!isLive(before) || before.awayScore > current->awayScore || before.homeScore > current->homeScore) {
        disarm(); return;
    }
    const uint16_t awayAdded = current->awayScore - before.awayScore;
    const uint16_t homeAdded = current->homeScore - before.homeScore;
    // Missing poll, multiple simultaneous goals, or uncertainty => fail closed.
    if (awayAdded + homeAdded > 1) { disarm(); return; }
    if (awayAdded + homeAdded == 0) return;
    if (pending_.valid || (cooldown_ && !reached(nowMs, cooldownUntil_))) return;
    pending_.valid = true;
    pending_.home = homeAdded == 1;
    pending_.favorite = pending_.home == favoriteHome_;
    pending_.gameId = gameId_;
    pending_.score = pending_.home ? current->homeScore : current->awayScore;
    dueAt_ = nowMs + uint32_t(delaySeconds_) * 1000U;
}
void GoalEngine::expire(uint32_t nowMs) {
    // Disconnect/stale feed: the first post-reconnect score must be a fresh
    // baseline established by the user, not an automatic event.
    if (armed_ && (uint32_t)(nowMs - lastFresh_) > 120000U) disarm();
    if (pending_.valid && reached(nowMs, dueAt_) &&
        (uint32_t)(nowMs-dueAt_) > 120000U) cancelPending();
}
bool GoalEngine::ready(uint32_t nowMs) const {
    if (!armed_ || !pending_.valid || !hasBaseline_) return false;
    // Never play a delayed event if the data became stale or an old timer
    // was carried across a very long sleep/blocking networking request.
    if ((uint32_t)(nowMs - lastFresh_) > 120000U) return false;
    if ((uint32_t)(nowMs - dueAt_) > 120000U && reached(nowMs, dueAt_)) return false;
    return reached(nowMs, dueAt_);
}
GoalEvent GoalEngine::consume(uint32_t nowMs) {
    if (!ready(nowMs)) return GoalEvent();
    GoalEvent out = pending_;
    pending_ = GoalEvent();
    cooldown_ = true;
    cooldownUntil_ = nowMs + 20000U;
    return out;
}
void ReactionController::service(uint32_t nowMs, bool networkOk) {
    if (!networkOk) engine_.disarm();
    engine_.expire(nowMs);
    if (lights_.needsRestore()) {
        if (active_ && reached(nowMs, endAt_)) active_ = false;
        // Disarm, Wi-Fi loss, expiry, or reboot requires recovery, not new effects.
        if (!active_ || !engine_.armed() || !networkOk) {
            active_ = false;
            if (networkOk && (!attemptedRestore_ || (uint32_t)(nowMs-lastRestoreTry_) >= 3000U)) {
                attemptedRestore_ = true;
                lastRestoreTry_ = nowMs;
                if (lights_.restore()) { ++restoreCount_; attemptedRestore_ = false; }
            }
        }
        return;
    }
    active_ = false;
    attemptedRestore_ = false;
    if (!networkOk || !engine_.ready(nowMs)) return;
    GoalEvent e = engine_.consume(nowMs);
    if (!e.valid) return;
    // No strobes, no effects with rapid flashes: solid color only.
    // The selected game's actual team RGB comes from setTeamColors.
    // See setTeamColors on the controller.
    uint32_t color = e.home ? homeColor_ : awayColor_;
    if (lights_.startColor(color)) {
        active_ = true;
        endAt_ = nowMs + 10000U;
        ++startedCount_;
    }
    // If startColor failed *after* snapshot, next tick retries recovery.
}
}
