#pragma once
#include <cmath>
namespace gd {
struct TouchTransform {
    float a=320.0f/3600, b=0, c=-200*320.0f/3600;
    float d=0, e=240.0f/3600, f=-200*240.0f/3600;
    bool fit(const int *x, const int *y) {
        const float ux=x[1]-x[0], uy=y[1]-y[0];
        const float vx=x[2]-x[0], vy=y[2]-y[0];
        const float det=ux*vy-uy*vx;
        if (std::fabs(det)<100000) return false;
        a=280*vy/det; b=-280*vx/det; c=20-a*x[0]-b*y[0];
        d=-200*uy/det; e=200*ux/det; f=20-d*x[0]-e*y[0];
        return true;
    }
    int screenX(int x,int y) const { return clamp(a*x+b*y+c,319); }
    int screenY(int x,int y) const { return clamp(d*x+e*y+f,239); }
private:
    static int clamp(float v,int max) { return v<0?0:v>max?max:static_cast<int>(v+0.5f); }
};
}
