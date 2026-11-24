#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../../tools/ui-render/glcdfont.h"

// Host-only adapter for the very same cyd::render() used by firmware.
// Text/rounded edges are simulated; output is not a physical LCD photograph.
struct CydHostCanvas {
    std::vector<uint16_t> pixels=std::vector<uint16_t>(320*240);
    int top=0,height=240,cursorX=0,cursorY=0,size=1;
    uint16_t ink=0;
    void at(int y,int h=24) { top=y; height=h; }
    bool visible(int y,int h) const { return y<top+height && y+h>top; }
    void pixel(int x,int y,uint16_t c) {
        if (x>=0 && x<320 && y>=0 && y<240 && y>=top && y<top+height) pixels[y*320+x]=c;
    }
    void fillRect(int x,int y,int w,int h,uint16_t c) {
        for (int yy=std::max(0,std::max(y,top));yy<std::min(240,std::min(y+h,top+height));++yy)
            for (int xx=std::max(0,x);xx<std::min(320,x+w);++xx) pixel(xx,yy,c);
    }
    void fillScreen(uint16_t c) { fillRect(0,0,320,240,c); }
    static bool roundContains(int x,int y,int w,int h,int r) {
        if (x<0 || y<0 || x>=w || y>=h) return false;
        r=std::min(r,std::min(w,h)/2);
        const int dx=x<r?r-1-x:x>=w-r?x-(w-r):0;
        const int dy=y<r?r-1-y:y>=h-r?y-(h-r):0;
        return dx*dx+dy*dy<=r*r;
    }
    void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c) {
        for (int yy=std::max(y,top);yy<std::min(y+h,top+height);++yy)
            for (int xx=x;xx<x+w;++xx) if (roundContains(xx-x,yy-y,w,h,r)) pixel(xx,yy,c);
    }
    void drawRoundRect(int x,int y,int w,int h,int r,uint16_t c) {
        for (int yy=std::max(y,top);yy<std::min(y+h,top+height);++yy)
            for (int xx=x;xx<x+w;++xx)
                if (roundContains(xx-x,yy-y,w,h,r) && !roundContains(xx-x-1,yy-y-1,w-2,h-2,r>0?r-1:0)) pixel(xx,yy,c);
    }
    void fillCircle(int x,int y,int r,uint16_t c) {
        for (int yy=std::max(y-r,top);yy<=std::min(y+r,top+height-1);++yy)
            for (int xx=x-r;xx<=x+r;++xx) if ((xx-x)*(xx-x)+(yy-y)*(yy-y)<=r*r) pixel(xx,yy,c);
    }
    void setTextColor(uint16_t c) { ink=c; }
    void setTextSize(int n) { size=n; }
    void setCursor(int x,int y) { cursorX=x; cursorY=y; }
    void print(const char *s) {
        for (;*s;++s) {
            if (*s=='\n') { cursorX=0; cursorY+=8*size; continue; }
            for (int col=0;col<5;++col) {
                const uint8_t bits=font[uint8_t(*s)*5+col];
                for (int row=0;row<8;++row) if (bits&(1<<row)) fillRect(cursorX+col*size,cursorY+row*size,size,size,ink);
            }
            cursorX+=6*size;
        }
    }
    void save(const char *path) const {
        FILE *f=std::fopen(path,"wb"); if (!f) std::abort();
        std::fprintf(f,"P6\n320 240\n255\n");
        for (uint16_t c:pixels) {
            const uint8_t rgb[3]={uint8_t(((c>>11)&31)*255/31),uint8_t(((c>>5)&63)*255/63),uint8_t((c&31)*255/31)};
            std::fwrite(rgb,1,3,f);
        }
        std::fclose(f);
    }
};
