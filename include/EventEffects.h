#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace gd {
enum class SportEvent : uint8_t { Goal, PowerPlay, PeriodStart, PeriodEnd, GameEnd, Overtime, Shootout };
constexpr int EVENT_COUNT=7, DDP_MAX_PIXELS=600, DDP_CHUNK=480;
inline const char* eventName(SportEvent e) {
    static const char* names[]={"GOAL","POWER PLAY","PERIOD START","PERIOD END","GAME END","OVERTIME","SHOOTOUT"};
    return names[static_cast<unsigned>(e)<7?static_cast<unsigned>(e):0];
}
inline int eventPriority(SportEvent e) {
    const int p[]={100,30,40,20,90,60,70};return p[static_cast<unsigned>(e)];
}
// Integer-only render contract, identical in Java and the browser preview.
inline uint32_t effectPixel(SportEvent e,uint32_t color,int i,int count,uint32_t ms) {
    unsigned level=255; const unsigned t=ms/50;
    switch(e) {
    case SportEvent::Goal: level=((i+int(t)*2)%24<8)?255:45;break;
    case SportEvent::PowerPlay: level=((i+int(t))%32<10)?220:30;break;
    case SportEvent::PeriodStart: level=(i<=int(t*unsigned(count)/80))?230:20;break;
    case SportEvent::PeriodEnd: level=255-(ms>8000?255:ms*255/8000);break;
    case SportEvent::GameEnd: level=((i*13+int(t)*3)%47<15)?255:70;break;
    case SportEvent::Overtime: {unsigned phase=t%40;level=45+(phase<20?phase:40-phase)*10;break;}
    case SportEvent::Shootout: level=((i/12+int(t)/12)%2)?240:35;break;
    }
    return (((color>>16)&255)*level/255<<16)|(((color>>8)&255)*level/255<<8)|((color&255)*level/255);
}
// DDP v1 RGB24, destination 1, byte offsets, PUSH only on final chunk.
inline size_t ddpPacket(uint8_t *out,size_t capacity,SportEvent e,uint32_t color,
                       int count,int first,uint32_t ms,uint8_t sequence) {
    if(count<1||count>DDP_MAX_PIXELS||first<0||first>=count)return 0;
    int n=count-first;if(n>DDP_CHUNK)n=DDP_CHUNK;
    const size_t bytes=size_t(n)*3;if(capacity<bytes+10)return 0;
    uint32_t offset=uint32_t(first)*3;
    out[0]=uint8_t(0x40|(first+n==count?1:0));out[1]=sequence&15;
    out[2]=0x0b;out[3]=1;
    out[4]=offset>>24;out[5]=offset>>16;out[6]=offset>>8;out[7]=offset;
    out[8]=bytes>>8;out[9]=bytes;
    for(int k=0;k<n;++k){uint32_t c=effectPixel(e,color,first+k,count,ms);
        out[10+k*3]=((c>>16)&255)*160/255;out[11+k*3]=((c>>8)&255)*160/255;out[12+k*3]=(c&255)*160/255;}
    return bytes+10;
}
// NHL situationCode = away goalie, away skaters, home skaters, home goalie.
// Both goalies must be present: never call an empty-net advantage a power play.
inline int advantage(const char* code) {
    if(!code||strlen(code)!=4||code[0]!='1'||code[3]!='1'||
       code[1]<'3'||code[1]>'5'||code[2]<'3'||code[2]>'5')return -1;
    return code[1]>code[2]?1:code[2]>code[1]?2:0;
}
inline SportEvent periodStart(const char* type) {
    return strcmp(type,"OT")==0?SportEvent::Overtime:strcmp(type,"SO")==0?SportEvent::Shootout:SportEvent::PeriodStart;
}
}
