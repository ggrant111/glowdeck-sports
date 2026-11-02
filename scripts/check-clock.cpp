#include "ClockFormat.h"
#include <cassert>
#include <cstring>
int main(){char b[12];
 gd::formatClock(b,sizeof(b),0,0,false);assert(!strcmp(b,"12:00 AM"));
 gd::formatClock(b,sizeof(b),12,0,false);assert(!strcmp(b,"12:00 PM"));
 gd::formatClock(b,sizeof(b),19,5,false);assert(!strcmp(b,"7:05 PM"));
 gd::formatClock(b,sizeof(b),0,5,true);assert(!strcmp(b,"00:05"));
 gd::formatClock(b,sizeof(b),23,59,true);assert(!strcmp(b,"23:59"));
 gd::formatClock(b,sizeof(b),23,59,false);assert(!strcmp(b,"11:59 PM"));
}
