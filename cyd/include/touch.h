#pragma once
#include <SPI.h>
#include <algorithm>

namespace cyd {
// XPT2046 pin map/sampling policy from the existing TRA board driver.
// SD is intentionally unused: its default VSPI mapping differs from touch.
class Touch {
    SPIClass spi{VSPI};
    int sample(uint8_t command) { spi.transfer(command); return (spi.transfer16(0)>>3)&4095; }
public:
    void begin() {
        pinMode(33,OUTPUT); digitalWrite(33,HIGH); pinMode(36,INPUT);
        spi.begin(25,39,32,33);
    }
    bool held() const { return digitalRead(36)==LOW; }
    bool raw(int &x,int &y) {
        if (!held()) return false;
        spi.beginTransaction(SPISettings(2000000,MSBFIRST,SPI_MODE0)); digitalWrite(33,LOW);
        const int z1=sample(0xb0),z2=sample(0xc0);
        int xs[5],ys[5];
        for (int i=0;i<5;++i) { xs[i]=sample(0xd0); ys[i]=sample(0x90); }
        sample(0x80); digitalWrite(33,HIGH); spi.endTransaction();
        if (z1+4095-z2<400) return false;
        std::sort(xs,xs+5); std::sort(ys,ys+5); x=xs[2]; y=ys[2];
        return x>0 && x<4095 && y>0 && y<4095;
    }
};
}
