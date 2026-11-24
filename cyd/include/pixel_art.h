#pragma once
#include "sprite.h"
#include "../../TamaPoke/species.h"
#include <cstdint>

namespace cyd {
// The approved 320x240 design uses 80x60 art cells (4 LCD pixels per cell),
// and an integer 2-pixel bitmap type size. Drawing is clipped by the render band.
constexpr int ArtPixel=4;
constexpr uint16_t rgb(uint32_t c) {
    return uint16_t(((c>>19)&31)<<11 | ((c>>10)&63)<<5 | ((c>>3)&31));
}
template<class C> void artRect(C &g,int x,int y,int w,int h,uint32_t color) {
    g.fillRect(x*ArtPixel,y*ArtPixel,w*ArtPixel,h*ArtPixel,rgb(color));
}
template<class C> void cutTile(C &g,int x,int y,int w,int h,int step,uint16_t color) {
    g.fillRect(x+step,y,w-step*2,h,color);g.fillRect(x,y+step,w,h-step*2,color);
}
template<class C> void artTile(C &g,int x,int y,int w,int h,uint32_t color) {
    cutTile(g,x*4,y*4,w*4,h*4,4,rgb(color));
}
template<class C,size_t N> void artMap(C &g,const char *const (&rows)[N],int x,int y,uint32_t color) {
    for(size_t row=0;row<N;++row) {
        if(!g.visible((y+int(row))*4,4))continue;
        for(int col=0;rows[row][col];++col)if(rows[row][col]=='1')artRect(g,x+col,y+int(row),1,1,color);
    }
}
inline const char *const *careIcon(unsigned i) {
    return i==0?SPR_ICON_FOOD:i==1?SPR_ICON_PLAY:i==2?SPR_ICON_LIGHT:SPR_ICON_CLEAN;
}
template<class C> void pixelIcon(C &g,const char *const *rows,int x,int y,int scale=4,bool small=false) {
    const int n=small?8:16;
    for(int row=0;row<n;++row) {
        if(!g.visible(y+row*scale,scale))continue;
        for(int col=0;col<n;++col) {
            const char ch=rows[small?row*2:row][small?col*2:col];
            if(ch!='.')g.fillRect(x+col*scale,y+row*scale,scale,scale,spriteColor(ch));
        }
    }
}
struct FrameBounds {int left=0,top=0,right=0,bottom=0;bool valid=false;};
inline FrameBounds frameBounds(const Sprite::Action &a,uint8_t frame) {
    FrameBounds b; b.left=a.w;b.top=a.h;
    const uint8_t *pixels=a.pixels+size_t(frame)*a.w*a.h;
    for(int y=a.top;y<a.base;++y)for(int x=a.left;x<a.right;++x)if(pixels[y*a.w+x]!=255) {
        if(x<b.left)b.left=x;
        if(y<b.top)b.top=y;
        if(x+1>b.right)b.right=x+1;
        if(y+1>b.bottom)b.bottom=y+1;
        b.valid=true;
    }
    return b;
}
template<class C> void pixelPet(C &g,const Sprite &sprite,uint8_t action,uint32_t now,int center=160,int ground=172,int scale=4) {
    const auto *a=sprite.action(action);if(!a)return;
    const uint8_t frame=a->frameAt(now);const auto b=frameBounds(*a,frame);if(!b.valid)return;
    while(scale>2&&((b.right-b.left)*scale>160||(b.bottom-b.top)*scale>128))scale-=2;
    const int x0=((center/scale)-(b.right-b.left+1)/2)*scale-b.left*scale;
    const int y0=ground-b.bottom*scale;
    const uint8_t *pixels=a->pixels+size_t(frame)*a->w*a->h;
    for(int row=b.top;row<b.bottom;++row) {
        if(!g.visible(y0+row*scale,scale))continue;
        for(int col=b.left;col<b.right;) {
            const int first=col;const uint8_t index=pixels[row*a->w+col++];
            while(col<b.right&&pixels[row*a->w+col]==index)++col;
            if(index!=255)g.fillRect(x0+first*scale,y0+row*scale,(col-first)*scale,scale,sprite.color(index));
        }
    }
}
// Precomputed from the approved stepped landscape; independent of frame time.
inline constexpr int8_t FarLine[80]={33,33,33,32,32,31,31,30,30,29,29,28,28,27,27,26,27,27,28,28,29,29,30,30,31,31,32,32,33,33,33,33,33,33,33,33,33,33,33,33,33,33,33,32,32,32,31,31,31,30,30,30,29,29,29,30,30,30,31,31,31,32,32,32,32,32,31,31,30,30,29,29,28,29,29,30,30,31,31,32};
inline constexpr int8_t GrassLine[80]={38,38,38,38,38,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,39,38,38,38,38,38,38,38,38,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,37,38,38,38,38,38,38,38,38,39,39,39,39,39,39};
template<class C> void pixelBackground(C &g,bool night) {
    constexpr uint32_t daySky[]={0xa2d8ed,0xa2d8ed,0xb3e0ed,0xc2e6e7};
    constexpr uint32_t darkSky[]={0x253552,0x304365,0x3c5673,0x466984};
    for(int row=0;row<40;++row)artRect(g,0,row,80,1,night?darkSky[row/10]:daySky[row/10]);
    if(night) {
        constexpr int stars[][2]={{8,13},{28,9},{40,15},{58,10},{74,23},{54,22}};
        for(const auto &p:stars)artRect(g,p[0],p[1],1,1,0xf6edc3);
        static const char *const moon[]={"0011100","0111000","1110000","1110000","1110000","0111000","0011100"};
        artMap(g,moon,65,13,0xf4e7ad);
    } else {
        static const char *const sun[]={"00111100","01111110","11111111","11111111","11111111","11111111","01111110","00111100"};
        static const char *const cloudA[]={"0001100000","0011110000","0111111100","1111111111","0111111110"};
        static const char *const cloudB[]={"00110000","01111000","11111111","01111110"};
        artMap(g,sun,65,12,0xf5e59e);artMap(g,cloudA,8,15,0xf2f7dd);artMap(g,cloudB,51,23,0xf2f7dd);
    }
    for(int x=0;x<80;++x) {
        artRect(g,x,FarLine[x],1,60-FarLine[x],night?0x3d655d:0x90c59c);
    }
    for(int x=0;x<80;++x) {
        artRect(g,x,GrassLine[x],1,60-GrassLine[x],night?0x305444:0x82b77b);
        artRect(g,x,GrassLine[x]+2,1,58-GrassLine[x],night?0x355c49:0x91c57f);
    }
    artRect(g,5,31,2,8,night?0x3e5944:0x857656);
    artTile(g,2,28,8,7,night?0x426b4e:0x71a675);artTile(g,3,25,6,5,night?0x4c7755:0x82b980);
    artRect(g,6,28,2,1,night?0x5a8463:0xa1cd92);
    constexpr int grass[][2]={{3,43},{16,40},{67,41},{74,46},{26,48},{57,48},{45,57},{7,56},{70,55}};
    for(const auto &p:grass) {
        artRect(g,p[0],p[1],1,2,night?0x244836:0x71a56b);artRect(g,p[0]-1,p[1]+1,3,1,night?0x244836:0x71a56b);
    }
    if(!night) {
        constexpr int flowers[][2]={{12,42},{65,39},{21,45},{75,40}};
        for(const auto &p:flowers) {
            artRect(g,p[0]-1,p[1],3,1,0xf5e7b6);artRect(g,p[0],p[1]-1,1,3,0xf5e7b6);artRect(g,p[0],p[1],1,1,0xd3b36b);
        }
    }
}
}
