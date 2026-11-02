#include "TouchTransform.h"
#include <cassert>
int main() {
    for(int swap=0;swap<2;++swap) for(int flip=0;flip<2;++flip) {
        int x[]={400,3600,400}, y[]={400,400,3600};
        for(int i=0;i<3;++i) { if(swap) {int t=x[i];x[i]=y[i];y[i]=t;} if(flip)x[i]=4000-x[i]; }
        gd::TouchTransform t; assert(t.fit(x,y));
        assert(t.screenX(x[0],y[0])==20 && t.screenY(x[0],y[0])==20);
        assert(t.screenX(x[1],y[1])==300 && t.screenY(x[1],y[1])==20);
        assert(t.screenX(x[2],y[2])==20 && t.screenY(x[2],y[2])==220);
        assert(t.screenX(2000,2000)==160 && t.screenY(2000,2000)==120);
    }
    gd::TouchTransform t;int x[]={1,2,3},y[]={1,2,3};assert(!t.fit(x,y));
    assert(t.screenX(-100,0)==0);assert(t.screenY(0,4095)==239);
}
