#pragma once
#include <cstdio>
#include <cstddef>
namespace gd {
inline void formatClock(char *out,size_t size,int hour,int minute,bool hour24) {
    if(hour24) std::snprintf(out,size,"%02d:%02d",hour,minute);
    else std::snprintf(out,size,"%d:%02d %s",hour%12?hour%12:12,minute,hour<12?"AM":"PM");
}
}
