#pragma once
#include <cmath>
#include <cstdint>

namespace cyd {
constexpr int Width = 320, Height = 240, BandHeight = 24;
struct Point { float x = 0, y = 0; };
struct Calibration {
    float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    bool valid() const {
        return std::isfinite(a) && std::isfinite(b) && std::isfinite(c) &&
               std::isfinite(d) && std::isfinite(e) && std::isfinite(f) &&
               std::fabs(a*e-b*d) > 0.00001f &&
               std::fabs(a) < 1 && std::fabs(b) < 1 && std::fabs(d) < 1 && std::fabs(e) < 1;
    }
    bool map(int rawX, int rawY, bool rotated, int &x, int &y) const {
        if (!valid() || rawX < 0 || rawX > 4095 || rawY < 0 || rawY > 4095) return false;
        const float px = a*rawX+b*rawY+c, py = d*rawX+e*rawY+f;
        if (px < -4 || px > Width+3 || py < -4 || py > Height+3) return false;
        x = int(std::lround(px)); y = int(std::lround(py));
        x = x < 0 ? 0 : x >= Width ? Width-1 : x;
        y = y < 0 ? 0 : y >= Height ? Height-1 : y;
        if (rotated) { x = Width-1-x; y = Height-1-y; }
        return true;
    }
};
constexpr Point Targets[4] = {{24,24}, {295,24}, {24,215}, {295,215}};
// Three-point affine fit plus an independent fourth-point acceptance check.
// Based on the existing TRA board calibration, with explicit finite/range checks.
inline bool fitCalibration(const Point raw[4], Calibration &out) {
    for (int i=0; i<4; ++i)
        if (!std::isfinite(raw[i].x) || !std::isfinite(raw[i].y) ||
            raw[i].x < 0 || raw[i].x > 4095 || raw[i].y < 0 || raw[i].y > 4095) return false;
    const float x0=raw[0].x, y0=raw[0].y, x1=raw[1].x, y1=raw[1].y, x2=raw[2].x, y2=raw[2].y;
    const float det=x0*(y1-y2)+x1*(y2-y0)+x2*(y0-y1);
    if (!std::isfinite(det) || std::fabs(det)<1000) return false;
    auto fit=[&](float z0,float z1,float z2,float &a,float &b,float &c) {
        a=(z0*(y1-y2)+z1*(y2-y0)+z2*(y0-y1))/det;
        b=(z0*(x2-x1)+z1*(x0-x2)+z2*(x1-x0))/det;
        c=(z0*(x1*y2-x2*y1)+z1*(x2*y0-x0*y2)+z2*(x0*y1-x1*y0))/det;
    };
    Calibration next;
    fit(Targets[0].x,Targets[1].x,Targets[2].x,next.a,next.b,next.c);
    fit(Targets[0].y,Targets[1].y,Targets[2].y,next.d,next.e,next.f);
    if (!next.valid()) return false;
    const float dx=next.a*raw[3].x+next.b*raw[3].y+next.c-Targets[3].x;
    const float dy=next.d*raw[3].x+next.e*raw[3].y+next.f-Targets[3].y;
    if (dx*dx+dy*dy > 18*18) return false;
    out=next;
    return true;
}
}
