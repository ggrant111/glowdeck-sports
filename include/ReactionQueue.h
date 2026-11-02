#pragma once
#include "EventEffects.h"
namespace gd {
constexpr unsigned FOLLOW_LIMIT=4, TARGET_LIMIT=2, QUEUE_LIMIT=8;
constexpr uint32_t QUEUE_TTL_MS=20000;
struct QueuedReaction { uint8_t game=0; SportEvent kind=SportEvent::Goal; bool home=false; uint32_t color=0, queuedAt=0; };
class ReactionQueue {
    QueuedReaction items_[QUEUE_LIMIT]; unsigned count_=0;
public:
    unsigned size() const {return count_;}
    void clear(){count_=0;}
    void removeGame(unsigned game){for(unsigned i=0;i<count_;)if(items_[i].game==game)erase(i);else ++i;}
    void expire(uint32_t now){for(unsigned i=0;i<count_;)if(uint32_t(now-items_[i].queuedAt)>QUEUE_TTL_MS)erase(i);else ++i;}
    bool push(QueuedReaction item,uint32_t now){
        expire(now);item.queuedAt=now;
        if(count_==QUEUE_LIMIT){unsigned low=0;for(unsigned i=1;i<count_;++i)if(eventPriority(items_[i].kind)<eventPriority(items_[low].kind))low=i;
            if(eventPriority(item.kind)<=eventPriority(items_[low].kind))return false;erase(low);}
        items_[count_++]=item;return true;
    }
    bool pop(uint32_t now,QueuedReaction &out){expire(now);if(!count_)return false;unsigned best=0;
        for(unsigned i=1;i<count_;++i)if(eventPriority(items_[i].kind)>eventPriority(items_[best].kind))best=i;
        out=items_[best];erase(best);return true;
    }
private:
    void erase(unsigned index){for(unsigned i=index+1;i<count_;++i)items_[i-1]=items_[i];--count_;}
};
}
