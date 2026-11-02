#include "GoalEngine.h"
#include <string.h>

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
    pending_ = GoalEvent(); finalSeen_=false;finalPending_=false;finalQueued_=false;
    armed_ = true; paused_ = true; baselineReady_ = false;
    lastEventId_ = 0; homeId_ = 0; awayId_ = 0;
    lastFresh_ = nowMs; cooldown_ = false;
    return true;
}
void GoalEngine::disarm() {
    finalPending_=false;
    armed_ = false; paused_ = false; baselineReady_ = false;
    cancelPending();
}
void GoalEngine::pause() {
    if (!armed_) return;
    finalPending_=false;
    paused_ = true; baselineReady_ = false; cancelPending();
}
void GoalEngine::observeGame(const Game *current) {
    bool final=current&&(strcmp(current->state,"OFF")==0||strcmp(current->state,"FINAL")==0);
    if (armed_ && (!current || current->id != gameId_ || (!isLive(*current)&&!(streaming_&&final)))) disarm();
}
void GoalEngine::observeFeed(const GoalFeed &feed, uint32_t nowMs) {
    if (!armed_) return;
    if (!feed.valid || feed.gameId != gameId_ || !feed.homeId || !feed.awayId ||
        feed.homeId == feed.awayId || feed.count > MAX_GOALS_PER_GAME || feed.extraCount>64) {
        pause(); return;
    }
    bool newlyFinal=feed.final&&!finalSeen_;
    if(newlyFinal){finalSeen_=true;finalAt_=nowMs;}
    if (paused_ || !baselineReady_ || feed.latestEventId < lastEventId_ ||
        homeId_ != feed.homeId || awayId_ != feed.awayId) {
        lastEventId_ = feed.latestEventId;
        homeId_ = feed.homeId; awayId_ = feed.awayId;
        lastFresh_ = nowMs; paused_ = false; baselineReady_ = true;
        return; // first successful sample is strictly a baseline
    }
    if(newlyFinal && !finalQueued_ && (eventMask_&(1U<<unsigned(SportEvent::GameEnd))))finalPending_=true;
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
    // Prefer a single eligible goal, then the highest-priority new non-goal cue.
    if(goals!=1 || !candidate.valid || !accepted(candidate.home,mode_) || !(eventMask_&1))candidate=GoalEvent();
    for(uint8_t i=0;i<feed.extraCount;++i){
        const ExtraPlay &p=feed.extra[i];
        if(p.id<=lastEventId_ || !(eventMask_&(1U<<unsigned(p.kind))))continue;
        if(p.kind==SportEvent::GameEnd){if(finalQueued_)continue;finalPending_=true;}
        bool home=p.ownerId==feed.homeId;
        if(p.ownerId && ((p.ownerId!=feed.homeId&&p.ownerId!=feed.awayId)||!accepted(home,mode_)))continue;
        if(candidate.valid && eventPriority(candidate.kind)>=eventPriority(p.kind))continue;
        candidate.valid=true;candidate.home=p.ownerId?home:mode_==CelebrationMode::Home;
        candidate.eventId=p.id;candidate.gameId=gameId_;candidate.kind=p.kind;
    }
    if(finalPending_ && (!candidate.valid || eventPriority(candidate.kind)<eventPriority(SportEvent::GameEnd))){
        candidate.valid=true;candidate.kind=SportEvent::GameEnd;candidate.home=mode_==CelebrationMode::Home;
        candidate.gameId=gameId_;candidate.eventId=feed.latestEventId;
    }
    lastFresh_ = nowMs;
    // Prevent duplicate and out-of-order events. NHL event IDs are expected
    // to increase; backwards feed resets are rebaselined above.
    if (feed.latestEventId > lastEventId_) lastEventId_ = feed.latestEventId;
    if (!candidate.valid ||
        pending_.valid || (cooldown_ && !reached(nowMs, cooldownUntil_))) return;
    if(candidate.kind==SportEvent::GameEnd){finalPending_=false;finalQueued_=true;}
    pending_ = candidate;
    dueAt_ = nowMs + uint32_t(delaySeconds_) * 1000U;
}
void GoalEngine::expire(uint32_t nowMs) {
    if(finalSeen_ && !pending_.valid && !finalPending_ && (!cooldown_ || reached(nowMs,cooldownUntil_)) && uint32_t(nowMs-finalAt_)>125000U)disarm();
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
    if (lights_.startEvent(e.home?homeColor_:awayColor_,e.kind)) {
        lastKind_=e.kind;
        active_=true; endAt_=nowMs+8000U; ++startedCount_;
        lastGoalHome_=e.home; lastEventId_=e.eventId;
    }
}
}
