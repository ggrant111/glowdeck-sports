#include "EventEffects.h"
#include <cstdio>
#include <initializer_list>
int main(){for(int e=0;e<7;e++)for(int ms:{0,50,1000,3950,7999})for(int i:{0,7,23,479,599})
 std::printf("%d %d %d %u\n",e,ms,i,gd::effectPixel(gd::SportEvent(e),0x6cace4,i,600,ms));}
