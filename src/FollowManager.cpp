#include "FollowManager.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <cstring>
void FollowManager::begin(){
    targets_[0].client.setJournalNamespace("gd-follow0");targets_[1].client.setJournalNamespace("gd-follow1");
    for(auto &t:targets_)t.client.begin();
    Preferences p;if(!p.begin("gd-follow",false))return;String raw=p.getString("config","");p.end();
    DynamicJsonDocument d(4096);if(deserializeJson(d,raw))return;
    for(unsigned t=0;t<gd::TARGET_LIMIT;++t)targetIp[t]=d["targets"][t]|"";
    for(JsonObject o:d["games"].as<JsonArray>()){
        if(count>=gd::FOLLOW_LIMIT)break;auto &f=games[count];f.game.id=o["id"]|0UL;
        if(f.game.id<2000000000UL||f.game.id>2999999999UL)continue;
        snprintf(f.game.away,sizeof(f.game.away),"%s",o["away"]|"?");snprintf(f.game.home,sizeof(f.game.home),"%s",o["home"]|"?");
        snprintf(f.game.state,sizeof(f.game.state),"%s",o["state"]|"FUT");
        f.display=o["display"]|true;f.sound=o["sound"]|false;f.targets=(o["targets"]|0)&3;f.ddp=o["ddp"]|false;
        f.mask=(o["mask"]|127)&127;f.delay=constrain(int(o["delay"]|0),0,90);f.mode=gd::validatedMode(o["mode"]|0);
        ++count;
    }
    message="Following preferences loaded; reactions stopped";
}
bool FollowManager::save(){
    DynamicJsonDocument d(4096);for(unsigned t=0;t<gd::TARGET_LIMIT;++t)d["targets"][t]=targetIp[t];
    for(unsigned i=0;i<count;++i){auto &f=games[i];JsonObject o=d["games"].createNestedObject();
        o["id"]=f.game.id;o["away"]=f.game.away;o["home"]=f.game.home;o["state"]=f.game.state;
        o["display"]=f.display;o["sound"]=f.sound;o["targets"]=f.targets;o["ddp"]=f.ddp;o["mask"]=f.mask;o["mode"]=int(f.mode);o["delay"]=f.delay;
    }
    if(d.overflowed())return false;String raw;serializeJson(d,raw);Preferences p;if(!p.begin("gd-follow",false))return false;
    bool ok=p.putString("config",raw)==raw.length();p.end();return ok;
}
bool FollowManager::busy()const{if(running)return true;for(auto &t:targets_)if(t.client.autoRestorePending())return true;return false;}
bool FollowManager::start(String &error){
    if(busy()){error="Stop / restore followed games first";return false;}
    if(!count){error="Follow at least one game";return false;}
    for(auto &ip:targetIp){ip.trim();IPAddress canonical;if(canonical.fromString(ip))ip=canonical.toString();}
    unsigned used=0;for(unsigned i=0;i<count;++i)used|=games[i].targets;
    if((used&3)==3&&targetIp[0]==targetIp[1]){error="Targets A and B must be different devices";return false;}
    for(unsigned t=0;t<gd::TARGET_LIMIT;++t)if(used&(1<<t)) {
        if(!targets_[t].client.configureTarget(targetIp[t])||!targets_[t].client.probe(0)){error="Selected WLED target unavailable";return false;}
    }
    for(unsigned i=0;i<count;++i){auto &f=games[i];f.engine.selectGame(f.game.id);f.engine.setMode(f.mode);f.engine.setDelay(f.delay);
        f.engine.setEventMask(f.ddp?f.mask:1,true);f.lastPoll=0;f.observedLive=false;f.status="Checking game / baseline";
        // Actual state is validated by the first game-ID feed before any event.
        gd::Game pending=f.game;strcpy(pending.state,"FUT");f.engine.arm(&pending,millis());
    }
    running=true;cursor_=0;nextPoll_=0;message="Followed games monitoring started";return true;
}
void FollowManager::stop(){running=false;sounds_.clear();for(unsigned i=0;i<count;++i){games[i].engine.disarm();games[i].status="Stopped";}
    for(auto &t:targets_){t.queue.clear();t.active=false;t.retry=0;}message="Stopped; restoring targets";
}
void FollowManager::dropGame(unsigned i){sounds_.removeGame(i);for(auto &t:targets_)t.queue.removeGame(i);}
void FollowManager::serviceTargets(bool online){
    const uint32_t now=millis();
    for(auto &t:targets_){t.queue.expire(now);if(!online){t.queue.clear();t.active=false;continue;}
        if(t.active && int32_t(now-t.ends)<0)continue;
        if(t.retry&&uint32_t(now-t.retry)<3000)continue;
        if(t.active)t.active=false;
        if(t.recovering){String err;if(t.client.restoreAuto(err)){t.recovering=false;t.retry=0;}else {t.retry=millis();message=err;}continue;}
        gd::QueuedReaction event;
        if(running&&t.queue.pop(now,event)){
            auto &f=games[event.game];if(f.engine.paused()||!f.engine.armed())continue;
            t.client.ddpEnabled=f.ddp;String err;
            bool ok=t.client.autoRestorePending()?t.client.continueAutoEvent(0,event.color,event.kind,err):t.client.beginAutoEvent(0,event.color,event.kind,err);
            if(ok){t.active=true;t.ends=millis()+8000;t.retry=0;if(f.display)message=String(f.game.away)+" @ "+f.game.home+": "+gd::eventName(event.kind);}
            else {t.queue.clear();t.recovering=true;t.retry=millis();message=err;}
            continue;
        }
        if(t.client.autoRestorePending()){String err;t.retry=millis();if(t.client.restoreAuto(err)){t.retry=0;message="Original WLED scene restored";}else message=err;}
    }
}
void FollowManager::tick(NhlClient &nhl,bool online){
    for(unsigned i=0;i<count;++i)games[i].engine.expire(millis());
    serviceTargets(online);if(!running)return;
    if(!online){sounds_.clear();for(unsigned i=0;i<count;++i){games[i].engine.pause();games[i].status="Reconnecting";}return;}
    uint32_t now=millis();
    if(!nextPoll_||int32_t(now-nextPoll_)>=0){
        unsigned index=cursor_++%count;auto &f=games[index];nextPoll_=now+6250;
        if(f.engine.armed()){
            static gd::GoalFeed feed;String err;
            if(nhl.pollGoals(f.game.id,feed,err,&f.game)){
                if(!f.observedLive && feed.final){f.engine.disarm();f.status="Finished; old events skipped";}
                else {f.engine.observeFeed(feed,millis());f.status=!f.engine.armed()?"Ended / cancelled":f.engine.waiting()?"Waiting for start":f.engine.paused()?"Waiting for feed":"Live monitoring";}
                if(gd::isLive(f.game))f.observedLive=true;
                f.lastPoll=millis();
            }else {f.engine.pause();f.status="Reconnecting: "+err;}
            if(f.engine.paused()||f.engine.waiting()||!f.engine.armed())dropGame(index);
        }
    }
    for(unsigned i=0;i<count;++i){auto &f=games[i];f.engine.expire(millis());if(!f.engine.armed()){dropGame(i);continue;}
        if(!f.engine.ready(millis()))continue;auto e=f.engine.consume(millis());
        gd::QueuedReaction q;q.game=i;q.home=e.home;q.kind=e.kind;q.color=gd::teamColor(e.home?f.game.home:f.game.away);
        for(unsigned t=0;t<gd::TARGET_LIMIT;++t)if(f.targets&(1<<t))if(!targets_[t].queue.push(q,millis()))message="Reaction queue full; event skipped";
        if(f.sound && e.kind==gd::SportEvent::Goal)sounds_.push(q,millis());
        if(f.display){f.status=String(gd::eventName(e.kind))+": "+(e.home?f.game.home:f.game.away);message=String(f.game.away)+" @ "+f.game.home+": "+gd::eventName(e.kind);}
    }
    bool any=false;for(unsigned i=0;i<count;++i)any|=games[i].engine.armed();if(!any)running=false;
    serviceTargets(online);
}
