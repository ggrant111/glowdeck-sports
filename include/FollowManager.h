#pragma once
#include "GoalEngine.h"
#include "ReactionQueue.h"
#include "NhlClient.h"
#include "WledClient.h"
struct FollowGame {
    gd::Game game;
    gd::GoalEngine engine;
    bool display=true,sound=false,ddp=false,observedLive=false;
    uint8_t targets=0,mask=127,delay=0;
    gd::CelebrationMode mode=gd::CelebrationMode::Away;
    uint32_t lastPoll=0;
    String status="Stopped";
};
class FollowManager {
public:
    FollowGame games[gd::FOLLOW_LIMIT];unsigned count=0;
    String targetIp[gd::TARGET_LIMIT],message;
    bool running=false;
    void begin();
    bool save();
    bool start(String &error);
    void stop();
    bool busy() const;
    void tick(NhlClient &nhl,bool online);
    bool takeSound(gd::QueuedReaction &out){return sounds_.pop(millis(),out);}
    void enqueueSound(gd::QueuedReaction e){sounds_.push(e,millis());}
private:
    struct Target { WledClient client;gd::ReactionQueue queue;bool active=false,recovering=false;uint32_t ends=0,retry=0; } targets_[gd::TARGET_LIMIT];
    gd::ReactionQueue sounds_;
    unsigned cursor_=0;uint32_t nextPoll_=0;
    void serviceTargets(bool online);
    void dropGame(unsigned i);
};
