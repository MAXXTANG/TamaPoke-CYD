#pragma once
#include "model.h"
#include "pixel_art.h"
#include "version.h"
#include "../../TamaPoke/dex.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace cyd {
constexpr uint16_t Ink=rgb(0x365b45),Paper=rgb(0xeee2b0),White=rgb(0xfff3d4),Track=rgb(0xc8c297);
constexpr uint16_t Yellow=rgb(0xf5e59e),Green=rgb(0x79a976),Red=rgb(0xcf7378),Blue=rgb(0xa2d8ed);
template<class C> void text(C &g,int x,int y,const char *s,int size=2,uint16_t color=Ink) {
    g.setTextColor(color);g.setTextSize(size);g.setCursor(x,y);g.print(s);
}
template<class C> void homeText(C &g,int x,int y,const char *s,uint16_t color=rgb(0xfff3ce),bool shadow=true) {
    if(shadow)text(g,x+2,y+2,s,2,rgb(0x305a58));
    text(g,x,y,s,2,color);
}
template<class C> void button(C &g,Rect r,const char *s,uint16_t color=White,int size=2) {
    cutTile(g,r.x,r.y+4,r.w,r.h,4,rgb(0x527e69));cutTile(g,r.x,r.y,r.w,r.h,4,rgb(0xb49c69));
    cutTile(g,r.x,r.y,r.w,r.h-4,4,color);
    text(g,r.x+(r.w-int(std::strlen(s))*6*size)/2,r.y+(r.h-8*size)/2,s,size);
}
template<class C> void title(C &g,const char *s) {
    g.fillRect(0,0,Width,32,Ink);text(g,8,8,s,2,White);
}
inline const char *decisionTitle(Decision d) {
    return d==Decision::Evolve?"Evolve now?":d==Decision::Farewell?"Say farewell?":
           d==Decision::Runaway?"Let them leave?":"Release your pet?";
}
template<class C> void homeScene(C &g,const Model &m,const Sprite &sprite,uint32_t now) {
    const Pet &p=m.pet;pixelBackground(g,p.sleeping);
    const bool recent=int32_t(m.toastUntil-now)>0;
    const unsigned active=p.sleeping?2:recent?m.homeAction:255;
    for(int y=0;y<3;++y)artRect(g,40-(10-y*2),42+y,20-y*4,1,p.sleeping?0x264938:0x64965d);
    if(p.isEgg()) {
        artTile(g,33,23,14,20,0x365b45);artTile(g,34,23,12,19,0xfff3d4);
        artRect(g,35,27,2,3,0x79a976);artRect(g,42,33,2,2,0x79a976);
        for(int i=0;i<p.eggCracks();++i)artRect(g,37+i,32+i,2,1,0x365b45);
    } else {
        const uint8_t action=p.sleeping?Sprite::Sleep:p.eating()?Sprite::Eat:p.showHeart()?Sprite::Pose:
            recent&&m.homeAction==3?Sprite::Sit:p.ceremony?Sprite::WalkRight:Sprite::Idle;
        pixelPet(g,sprite,action,now);
        if(!sprite.loaded)homeText(g,112,112,"NO SPRITE");
        if(p.showHeart()) {
            static const char *const heart[]={"11011","11111","11111","01110","00100"};
            artMap(g,heart,54,23,0xcf7378);
        }
        if(p.eating())pixelIcon(g,SPR_ICON_FOOD,212,108,4);
        if(recent&&m.homeAction==3) {
            static const char *const bubble[]={"0110","1111","1111","0110"};
            constexpr int places[][2]={{26,26},{52,30},{24,36}};
            for(const auto &q:places){artMap(g,bubble,q[0],q[1],0xd9eddf);artRect(g,q[0]+1,q[1]+1,1,1,0xf6f7d9);}
        }
    }
    for(unsigned i=0;i<4;++i) {
        const int x=2+i*20,y=44;const bool pressed=active==i;
        artTile(g,x,y+1,16,14,p.sleeping?0x253f33:0x4f7e4d);
        artTile(g,x,y,16,14,pressed?0xd7bf83:0xb49c69);
        artTile(g,x,y,16,13,pressed?0xefd28b:0xf3e7b6);
        artRect(g,x+2,y+1,12,1,pressed?0xf5df9c:0xfff3d4);
        pixelIcon(g,careIcon(i),x*4,(y-1)*4,4);
    }
    artTile(g,70,2,8,8,0x5c8f85);artRect(g,71,3,6,6,0x7eada0);
    char name[9],level[16];
    const char *base=p.isEgg()?"EGG":p.nick[0]?p.nick:
        p.speciesId>=1&&p.speciesId<=DEX_COUNT?DEX_TBL[p.speciesId].name:"PET";
    std::snprintf(name,sizeof(name),"%.8s",base);std::snprintf(level,sizeof(level),"Lv.%u",p.level());
    homeText(g,8,8,name);homeText(g,8,28,level);homeText(g,290,14,"i",rgb(0xfff3d2),false);
    const uint8_t values[]={p.fullness,p.joy,p.energy,p.hygiene};
    for(unsigned i=0;i<4;++i) {
        const int x=116+i*36;pixelIcon(g,careIcon(i),x,8,4,true);
        g.fillRect(x,44,32,4,rgb(0x619b91));
        const int fill=(int(values[i])*8+50)/100*4;
        if(fill)g.fillRect(x,44,fill,4,rgb(0xf7e5a3));
    }
    const char *status=p.evolving()?"EVOLVE!":p.sleeping?"ZZZ...":p.eating()?"YUM!":
        p.showHeart()?"LOVE!":recent&&m.homeAction==3?"FRESH!":nullptr;
    if(status)homeText(g,224,96,status);
    if(recent&&m.homeAction==255&&m.toast[0])
        homeText(g,(Width-int(std::strlen(m.toast))*12)/2,56,m.toast);
    if(p.wantEvolveButton())homeText(g,96,152,"TAP: EVOLVE");
    else if(p.wantFarewellButton())homeText(g,104,152,"TAP: BYE");
    else if(p.canRunawayNow())homeText(g,96,152,"TAP: STAY?");
    if(p.ceremony)homeText(g,104,152,"GOODBYE!");
    if(m.details) {
        g.fillRect(64,46,192,128,rgb(0x527e69));g.fillRect(64,42,192,128,Paper);
        text(g,76,50,"STATUS");text(g,230,50,">");
        const char *labels[]={"FOOD","JOY","ENERGY","CLEAN"};char buf[8];
        for(int i=0;i<4;++i) {
            const int y=74+i*20;text(g,76,y,labels[i]);g.fillRect(154,y+4,56,8,Track);
            const int fill=(int(values[i])*28+50)/100*2;if(fill)g.fillRect(154,y+4,fill,8,Green);
            std::snprintf(buf,sizeof(buf),"%u",values[i]);text(g,218,y,buf);
        }
    }
}
template<class C> void rhythmScene(C &g,const Model &m,const Sprite &sprite,uint32_t now) {
    const Rhythm &r=m.rhythm;pixelBackground(g,false);
    const bool feedback=r.showingFeedback(now),perfect=feedback&&r.feedback==Rhythm::Perfect;
    const float progress=perfect?float(now-r.feedbackAt)/Rhythm::FeedbackMs:0;
    const float reach=perfect?std::sin(progress*3.14159265f):0;
    const uint8_t action=perfect?Sprite::Attack:!r.begun?Sprite::Idle:
        r.phase(now)>Rhythm::BeatMs/2?Sprite::Pose:Sprite::Nod;
    for(int y=0;y<2;++y)artRect(g,24,43+y,14,1,0x64965d);
    pixelPet(g,sprite,action,perfect?now-r.feedbackAt:r.elapsed(now),
        (31+int(std::lround(9*reach)))*4,172-int(std::lround(4*reach))*4);
    artTile(g,53,23,12,12,r.window(now)?0xf7e5a3:0x79a976);
    static const char *const hand[]={"0101010","0101010","0101010","1111110","1111111","1111111","0111110","0011100"};
    constexpr int outline[][2]={{-1,0},{1,0},{0,-1},{0,1}};
    for(const auto &o:outline)artMap(g,hand,56+o[0],25+o[1],0x365b45);
    artMap(g,hand,56,25,0xfff3d4);
    if(perfect) {
        static const char *const heart[]={"11011","11111","11111","01110","00100"};
        static const char *const star[]={"00100","01110","11111","01110","10101"};
        artMap(g,heart,46,17,0xcf7378);
        for(int i=0;i<5;++i) {
            const float a=i*6.2831853f/5;
            const int x=50+int(std::lround(std::cos(a)*(6+progress*7)));
            const int y=23+int(std::lround(std::sin(a)*(5+progress*5)));
            artMap(g,star,x-2,y-2,0xfff3b1);
        }
    }
    artRect(g,7,45,66,2,0x619b91);artRect(g,48,45,20,2,0xf7e5a3);
    artRect(g,7+int(r.phase(now)*64/Rhythm::BeatMs),44,2,4,0xfff3d4);
    button(g,RhythmBack,"BACK",Paper);
    button(g,RhythmAction,r.begun?"HIGH FIVE!":"START BEAT",White);
    homeText(g,8,8,"HIGH FIVE BEAT");
    char buf[16];std::snprintf(buf,sizeof(buf),"%u/12",r.hits);homeText(g,260,8,buf);
    const char *status=!r.begun?"TAP TO START":feedback?
        r.feedback==Rhythm::Perfect?"PERFECT!":r.feedback==Rhythm::Early?"TOO EARLY":
        r.feedback==Rhythm::Late?"TOO LATE":"MISSED!":r.window(now)?"HIGH FIVE NOW!":"WAIT FOR THE BEAT";
    homeText(g,(Width-int(std::strlen(status))*12)/2,34,status);
    if(r.begun){std::snprintf(buf,sizeof(buf),"C%u",r.combo);homeText(g,8,64,buf);}
}
template<class C> void gameStar(C &g,int x,int y,bool outline=true) {
    static const char *const star[]={"0001000","0011100","1111111","0111110","0011100","0110110","1100011"};
    const int cx=(x+2)/4-3,cy=(y+2)/4-3;
    if(outline){artMap(g,star,cx,cy+1,0x977e49);artMap(g,star,cx-1,cy,0x977e49);artMap(g,star,cx+1,cy,0x977e49);}
    artMap(g,star,cx,cy,0xf7e5a3);artRect(g,cx+3,cy+2,1,1,0xfff3d4);
}
template<class C> void gameHeader(C &g,const char *name,uint16_t score,uint32_t left,const char *status,const char *corner) {
    homeText(g,8,8,name);char buf[16];std::snprintf(buf,sizeof(buf),"%lus",(unsigned long)((left+999)/1000));homeText(g,260,8,buf);
    std::snprintf(buf,sizeof(buf),"S%u",score);homeText(g,8,34,buf);homeText(g,280,34,corner);
    homeText(g,(Width-int(std::strlen(status))*12)/2,34,status);
}
template<class C> void starScene(C &g,const Model &m,const Sprite &sprite,uint32_t now) {
    const auto &s=m.stars;pixelBackground(g,false);
    const uint32_t age=now-s.feedbackAt;
    const bool caught=s.feedback==StarGame::Collected&&age<StarGame::CollectMs;
    const bool missed=s.feedback==StarGame::Missed&&age<StarGame::MissMs;
    const bool aim=s.feedback==StarGame::Aim&&age<350;
    const int hero=s.heroX(now);
    const int hop=caught?int(std::lround(6*std::sin(float(age)*3.14159265f/StarGame::CollectMs)))*4:0;
    const uint8_t action=caught?age<200?(s.toX>=s.fromX?Sprite::WalkRight:Sprite::WalkLeft):Sprite::Hop:
        missed?Sprite::Hurt:Sprite::Idle;
    artTile(g,hero/4-8,43,16,2,0x64965d);
    pixelPet(g,sprite,action,caught||missed?age:s.elapsed(now),hero,172-hop);
    if(!s.waiting) {
        const int x=s.targetX,y=s.targetY(now);
        if(s.begun){artRect(g,x/4,y/4-8,1,1,0xf2f7dd);artRect(g,x/4-1,y/4-6,1,2,0xf5e7b6);}
        gameStar(g,x,y);
        if(s.begun){const uint32_t spent=now-s.targetAt;const int remaining=int(s.lifeMs-(spent<s.lifeMs?spent:s.lifeMs));
            g.fillRect(x-20,y+24,40,4,rgb(0x619b91));g.fillRect(x-20,y+24,remaining*10/int(s.lifeMs)*4,4,Yellow);}
    } else if(caught) {
        const int x=s.hitX+(s.toX-s.hitX)*int(age)/int(StarGame::CollectMs);
        const int y=s.hitY+(128-s.hitY)*int(age)/int(StarGame::CollectMs)-hop;
        gameStar(g,x,y);
        for(int i=0;i<4;++i){const int dx=(i&1)?1:-1,dy=(i&2)?1:-1;
            artRect(g,x/4+dx*(4+int(age)/80),y/4+dy*(3+int(age)/100),1,1,0xfff3d4);}
    }
    button(g,RhythmBack,"BACK",Paper);
    if(!s.begun)button(g,RhythmAction,"START CATCH",White);
    else text(g,120,204,"TAP THE STAR");
    char corner[8];std::snprintf(corner,sizeof(corner),"C%u",s.combo);
    const char *status=!s.begun?"TAP FALLING STARS":caught?s.combo>=3?"GREAT COMBO!":"GOT IT!":
        missed?"NEXT ONE!":aim?"AIM FOR THE STAR":"CATCH!";
    const uint32_t elapsed=s.elapsed(now);gameHeader(g,"CATCH THE STARS",s.score,elapsed<StarGame::Duration?StarGame::Duration-elapsed:0,status,corner);
}
template<class C> void strengthScene(C &g,const Model &m,const Sprite &sprite,uint32_t now) {
    const auto &s=m.strength;pixelBackground(g,false);
    const uint32_t age=now-s.lastPunchAt;const bool impact=s.impact(now),broken=s.broken(now);
    const float phase=impact?float(age)/StrengthGame::CooldownMs:0;
    const int lunge=impact?int(std::lround(9*std::sin(phase*3.14159265f))):0;
    artTile(g,19+lunge,43,16,2,0x64965d);
    pixelPet(g,sprite,broken?Sprite::Pose:impact?Sprite::Attack:Sprite::Idle,impact||broken?age:s.elapsed(now),(27+lunge)*4,172);
    const int sway=impact?int(std::lround(std::sin(phase*9.424778f)*(1-phase)*s.power)):0;
    artRect(g,42,13,28,1,0x365b45);
    for(int y=14;y<19;++y)artRect(g,56+sway*(y-13)/5,y,1,1,0x857656);
    const int left=49+sway;
    if(broken) {
        const int spread=int(age/100);artTile(g,left-2-spread,23+spread,5,8,0xa75955);
        artTile(g,left+6+spread,28+spread,6,7,0xcf8c6d);artTile(g,left+3,35,5,6,0x89724f);
        for(int i=0;i<4;++i)gameStar(g,(left+2+i*4)*4,(20+(i%2)*5)*4,false);
    } else {
        artTile(g,left,19,14,23,s.bags%2?0x89724f:0xa75955);
        artRect(g,left+2,20,2,20,s.bags%2?0xc29e62:0xcf8c6d);
        artRect(g,left,25,14,1,0x365b45);artRect(g,left,36,14,1,0x365b45);
        if(s.hp<=8){artRect(g,left+8,27,1,3,0xeee2b0);artRect(g,left+7,29,1,2,0xeee2b0);}
        if(s.hp<=4){artRect(g,left+7,31,2,1,0xeee2b0);artRect(g,left+9,32,1,3,0xeee2b0);}
        if(impact){gameStar(g,(left-1)*4,108,false);char points[8];std::snprintf(points,sizeof(points),"+%u",s.power);homeText(g,(left+4)*4,76-int(age/32)*2,points);}
    }
    artRect(g,50,16,12,1,0x619b91);if(s.hp)artRect(g,50,16,s.hp,1,0xf7e5a3);
    const uint32_t charge=s.charge(now);artRect(g,7,45,66,2,0x619b91);
    if(charge)artRect(g,7,45,int(charge*66/100),2,charge==100?0xf7e5a3:0xfff3d4);
    artRect(g,50,45,1,2,0x79a976);
    button(g,RhythmBack,"BACK",Paper);button(g,RhythmAction,s.begun?"PUNCH!":"START TRAIN",charge==100?Yellow:White);
    const char *status=!s.begun?"CHARGE, THEN PUNCH":broken?"BAG DOWN!":impact?s.power==3?"POWER +3!":s.power==2?"NICE +2!":"HIT +1!":charge==100?"FULL POWER!":"CHARGING...";
    char corner[8];std::snprintf(corner,sizeof(corner),"B%u",s.bags);
    const uint32_t elapsed=s.elapsed(now);gameHeader(g,"POWER TRAINING",s.score,elapsed<StrengthGame::Duration?StrengthGame::Duration-elapsed:0,status,corner);
}
template<class C> void render(C &g,const Model &m,const Sprite &sprite,uint32_t now,bool clockValid,uint32_t) {
    const Pet &p=m.pet;char buf[64];
    if(m.page==Page::Home){homeScene(g,m,sprite,now);return;}
    if(m.page==Page::Rhythm){rhythmScene(g,m,sprite,now);return;}
    if(m.page==Page::Catch){starScene(g,m,sprite,now);return;}
    if(m.page==Page::Training){strengthScene(g,m,sprite,now);return;}
    g.fillScreen(Paper);
    if(m.page==Page::Food) {
        title(g,"A LITTLE SNACK");
        const char *labels[]={"RED BERRY","BLUE BERRY","GREEN BERRY","CANDY"};
        const char *const *icons[]={SPR_ICON_FOOD,SPR_ICON_BERRY_B,SPR_ICON_BERRY_G,SPR_ICON_CANDY};
        for(int i=0;i<4;++i) {
            const auto r=FoodButtons[i];button(g,r,"",White);pixelIcon(g,icons[i],r.x+(r.w-32)/2,r.y+2,2);
            text(g,r.x+(r.w-int(std::strlen(labels[i]))*12)/2,r.y+36,labels[i]);
        }
    } else if(m.page==Page::Games) {
        title(g,"LET'S PLAY");button(g,GameButtons[0],"HIGH FIVE BEAT",Yellow);
        button(g,GameButtons[1],"CATCH THE STARS",White);
        button(g,GameButtons[2],"TRAIN STRENGTH",Blue);
    } else if(m.page==Page::Result) {
        title(g,"NICE PLAYING!");std::snprintf(buf,sizeof(buf),"%u",m.score);text(g,128,56,buf,4);
        text(g,76,108,m.trainingResult?"TRAINING DONE":m.rhythmResult?"HIGH FIVES":"STARS CAUGHT");
        if(m.trainingResult){std::snprintf(buf,sizeof(buf),"Strength +%u",m.reward);text(g,64,148,buf);
            std::snprintf(buf,sizeof(buf),"BAGS %u",m.strength.bags);text(g,112,168,buf);}
        else if(m.rhythmResult){std::snprintf(buf,sizeof(buf),"BEST COMBO %u",m.rhythm.bestCombo);text(g,64,148,buf);}
        else {std::snprintf(buf,sizeof(buf),"BEST COMBO %u",m.stars.bestCombo);text(g,64,148,buf);text(g,64,168,"JOY + SPEED UP");}
    } else if(m.page==Page::Info) {
        title(g,"YOUR LITTLE FRIEND");
        const char *name=p.isEgg()?"EGG":p.speciesId>=1&&p.speciesId<=DEX_COUNT?DEX_TBL[p.speciesId].name:"PET";
        std::snprintf(buf,sizeof(buf),"%.11s  Lv.%u",name,p.level());text(g,8,44,buf);
        std::snprintf(buf,sizeof(buf),"ATK %u DEF %u SPD %u",p.atkStat(),p.defStat(),p.speStat());text(g,8,74,buf);
        std::snprintf(buf,sizeof(buf),"BOND %u   WEIGHT %u",p.bond,p.weight);text(g,8,100,buf);
        std::snprintf(buf,sizeof(buf),"STREAK %u  MISTAKES %u",p.streak,p.careMistakes);text(g,8,126,buf);
        std::snprintf(buf,sizeof(buf),"BEST %u   TRAIN %u",p.gameHi,p.strHi);text(g,8,152,buf);
        button(g,InfoDex,"DEX",Yellow);button(g,InfoSetup,"SETUP",Blue);
    } else if(m.page==Page::Settings) {
        title(g,"SETTINGS");
        for(int i=0;i<3;++i) {
            const int level=i==0?20:i==1?50:100;std::snprintf(buf,sizeof(buf),"%d%%",level);
            button(g,{12+i*102,50,92,38},buf,m.brightness==level?Yellow:White);
        }
        button(g,{12,104,142,40},m.rotated?"ROTATE:180":"ROTATE:0",White,2);
        button(g,{166,104,142,40},"CALIBRATE",White,2);
        text(g,12,160,clockValid?"TIME: USB SET":"TIME: NOT SET");
        std::snprintf(buf,sizeof(buf),"v%s",Version);text(g,112,204,buf);
    } else if(m.page==Page::Dex) {
        title(g,"POKEDEX");std::snprintf(buf,sizeof(buf),"SEEN %u / 151",p.registeredCount());text(g,12,46,buf);
        text(g,12,80,p.isRegistered(25)?"#025 PIKACHU  OK":"#025 PIKACHU   ?");
        text(g,12,108,p.isRegistered(26)?"#026 RAICHU   OK":"#026 RAICHU    ?");
        text(g,12,148,"PIKACHU + RAICHU");
    } else if(m.page==Page::Confirm) {
        title(g,"A MOMENT TO DECIDE");text(g,20,56,decisionTitle(m.decision));
        text(g,20,90,m.decision==Decision::Evolve?"READY TO GROW.":"A NEW EGG WILL FOLLOW.");
        button(g,ConfirmYes,"YES",m.decision==Decision::Release?Red:Yellow);button(g,ConfirmNo,"KEEP",White);return;
    }
    button(g,Back,"BACK");
}
}
