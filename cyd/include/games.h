#pragma once
#include <Arduino.h>
#include <cstdint>

namespace cyd {
struct StarGame {
    static constexpr uint32_t Duration=20000,CollectMs=420,MissMs=500;
    enum Feedback : uint8_t { None,Collected,Missed,Aim };
    bool begun=false,running=false,waiting=false;
    uint16_t score=0;
    uint8_t combo=0,bestCombo=0,misses=0;
    uint32_t startedAt=0,targetAt=0,feedbackAt=0,lifeMs=2200;
    int targetX=232,startY=80,fromX=104,toX=104,hitX=232,hitY=80;
    Feedback feedback=None;
    void reset(){*this=StarGame();}
    uint32_t elapsed(uint32_t now)const{return begun?now-startedAt:0;}
    bool finished()const{return begun&&!running;}
    int heroX(uint32_t now)const {
        const uint32_t age=now-feedbackAt;
        return feedback==Collected&&age<CollectMs?fromX+(toX-fromX)*int(age)/int(CollectMs):toX;
    }
    int targetY(uint32_t now)const {
        const uint32_t age=begun?now-targetAt:0;
        return startY+int((age>lifeMs?lifeMs:age)*52/lifeMs);
    }
    void next(uint32_t now) {
        targetX=48+int(random(57))*4;startY=72+int(random(5))*4;
        targetAt=now;waiting=false;
        const uint32_t stage=elapsed(now)/5000;lifeMs=2200-(stage>3?3:stage)*200;
    }
    void start(uint32_t now){reset();begun=running=true;startedAt=now;next(now);}
    void advance(uint32_t now) {
        if(!running)return;
        if(elapsed(now)>=Duration){running=false;return;}
        if(waiting) {
            if(now-feedbackAt>=(feedback==Collected?CollectMs:MissMs))next(now);
        } else if(now-targetAt>=lifeMs) {
            ++misses;combo=0;feedback=Missed;feedbackAt=now;waiting=true;
        }
    }
    bool tap(int x,int y,uint32_t now) {
        advance(now);if(!running||waiting)return false;
        const int dx=x-targetX,dy=y-targetY(now);
        if(dx<-28||dx>28||dy<-28||dy>28){feedback=Aim;feedbackAt=now;return false;}
        fromX=heroX(now);toX=targetX<68?68:targetX>252?252:targetX;
        hitX=targetX;hitY=targetY(now);feedback=Collected;feedbackAt=now;waiting=true;
        ++score;++combo;if(combo>bestCombo)bestCombo=combo;return true;
    }
};
struct StrengthGame {
    static constexpr uint32_t Duration=15000,ChargeMs=900,CooldownMs=400,BreakMs=700;
    static constexpr uint8_t BagHealth=12;
    bool begun=false,running=false;
    uint16_t score=0,hits=0;
    uint8_t power=0,hp=BagHealth,bags=0;
    uint32_t startedAt=0,lastPunchAt=0;
    void reset(){*this=StrengthGame();}
    void start(uint32_t now){reset();begun=running=true;startedAt=lastPunchAt=now;}
    uint32_t elapsed(uint32_t now)const{return begun?now-startedAt:0;}
    bool finished()const{return begun&&!running;}
    uint32_t charge(uint32_t now)const {
        if(!begun)return 0;
        const uint32_t age=now-lastPunchAt;
        return age>=ChargeMs?100:age*100/ChargeMs;
    }
    bool impact(uint32_t now)const{return hits&&now-lastPunchAt<CooldownMs;}
    bool broken(uint32_t now)const{return hits&&hp==0&&now-lastPunchAt<BreakMs;}
    void advance(uint32_t now) {
        if(!running)return;
        if(elapsed(now)>=Duration){running=false;return;}
        if(hp==0&&now-lastPunchAt>=BreakMs)hp=BagHealth;
    }
    bool tap(uint32_t now) {
        advance(now);if(!running||broken(now)||now-lastPunchAt<CooldownMs)return false;
        const uint32_t value=charge(now);power=value==100?3:value>=66?2:1;
        ++hits;score+=power;lastPunchAt=now;
        if(power>=hp){hp=0;++bags;}else hp-=power;
        return true;
    }
};
}
