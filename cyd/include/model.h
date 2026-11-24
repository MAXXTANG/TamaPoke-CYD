#pragma once
#include "../../TamaPoke/pet.h"
#include "calibration.h"
#include "rhythm.h"
#include "games.h"

namespace cyd {
enum class Page : uint8_t { Home, Food, Games, Catch, Training, Result, Info, Settings, Dex, Confirm, Rhythm };
enum class Decision : uint8_t { Evolve, Farewell, Runaway, Release };
struct Rect {
    int x,y,w,h;
    bool contains(int px,int py) const { return px>=x && py>=y && px<x+w && py<y+h; }
};
constexpr Rect Nav[4]={{8,176,64,56},{88,176,64,56},{168,176,64,56},{248,176,64,56}};
constexpr Rect Back={8,188,88,44}, PetArea={96,56,128,116}, InfoButton={272,0,48,48};
constexpr Rect StatusPanel={64,42,192,128}, ProfileButton={208,42,48,44};
constexpr Rect FoodButtons[4]={{12,48,142,56},{166,48,142,56},{12,118,142,56},{166,118,142,56}};
constexpr Rect GameButtons[3]={{16,40,288,44},{16,88,288,44},{16,136,288,44}};
constexpr Rect RhythmBack={8,192,72,44},RhythmAction={96,192,216,44},RhythmField={0,48,320,136};
constexpr Rect ConfirmYes={24,134,128,48}, ConfirmNo={168,134,128,48};
constexpr Rect InfoDex={104,188,96,44}, InfoSetup={208,188,104,44};

class Model {
public:
    Pet &pet;
    Page page=Page::Home;
    Decision decision=Decision::Evolve;
    bool rotated=false, rotateRequested=false, calibrateRequested=false, settingsDirty=false, details=false;
    uint8_t brightness=50;
    uint16_t score=0;
    uint8_t misses=0, reward=0;
    uint32_t gameStart=0, toastUntil=0;
    int targetX=80,targetY=80;
    const char *toast="";
    uint8_t homeAction=255;
    bool trainingResult=false,rhythmResult=false;
    Rhythm rhythm;
    StarGame stars;
    StrengthGame strength;
    explicit Model(Pet &p):pet(p){}
    void message(const char *text,uint32_t now,uint8_t action=255) { toast=text; toastUntil=now+1800; homeAction=action; }
    void home() { page=Page::Home; details=false; rhythm.reset();stars.reset();strength.reset(); }
    bool playing()const {
        return (page==Page::Rhythm&&rhythm.running)||(page==Page::Catch&&stars.running)||
            (page==Page::Training&&strength.running);
    }
    void openRhythm(uint32_t now) {
        if(pet.isEgg()||pet.sleeping||pet.ceremony){message("Wake up to play",now);return;}
        page=Page::Rhythm;rhythm.reset();score=reward=0;trainingResult=false;rhythmResult=true;
    }
    bool openGame(bool training,uint32_t now) {
        if (pet.isEgg() || pet.sleeping || pet.ceremony) { message("Wake up to play",now); return false; }
        page=training?Page::Training:Page::Catch;
        trainingResult=training;rhythmResult=false;score=0;misses=0;reward=0;gameStart=now;
        stars.reset();strength.reset();targetX=stars.targetX;targetY=stars.startY;return true;
    }
    void startGame(bool training,uint32_t now) {
        if(!openGame(training,now))return;
        if(training)strength.start(now);else {stars.start(now);targetX=stars.targetX;targetY=stars.targetY(now);}
    }
    void finishGame() {
        if (page!=Page::Catch && page!=Page::Training && page!=Page::Rhythm) return;
        if (page==Page::Rhythm && !rhythm.finished()) return;
        if (page==Page::Catch && !stars.finished()) return;
        if (page==Page::Training && !strength.finished()) return;
        if (page==Page::Training) reward=pet.trainStrength(score);
        else { pet.playResult(uint8_t(score>255?255:score)); reward=uint8_t(score>20?20:score); }
        page=Page::Result;
    }
    void tick(uint32_t now) {
        if(page==Page::Rhythm) {
            rhythm.advance(now);score=rhythm.hits;
            if(rhythm.finished())finishGame();
        }
        if(page==Page::Catch) {
            stars.advance(now);score=stars.score;misses=stars.misses;targetX=stars.targetX;targetY=stars.targetY(now);
            if(stars.finished())finishGame();
        }
        if(page==Page::Training){strength.advance(now);score=strength.score;if(strength.finished())finishGame();}
    }
    void hold(uint32_t now) {
        if (details) { details=false; return; }
        if (page==Page::Home && !pet.isEgg() && !pet.ceremony) {
            decision=Decision::Release; page=Page::Confirm;
        } else message("Hold the pet on home",now);
    }
    void tap(int x,int y,uint32_t now) {
        if (x<0 || y<0 || x>=Width || y>=Height) return;
        if (page==Page::Confirm) {
            if (ConfirmNo.contains(x,y)) { home(); return; }
            if (ConfirmYes.contains(x,y)) {
                if (decision==Decision::Evolve) { if (pet.canEvolveNow()) pet.evolve(); }
                else if (decision==Decision::Farewell) { if (pet.canFarewellNow()) pet.startFarewell(); }
                else if (decision==Decision::Runaway) { if (pet.canRunawayNow()) pet.startRunaway(); }
                else pet.release();
                home();
            }
            return;
        }
        if(page==Page::Rhythm) {
            if(RhythmBack.contains(x,y)){home();return;}
            if(RhythmAction.contains(x,y)||RhythmField.contains(x,y)) {
                if(!rhythm.begun)rhythm.start(now);
                else rhythm.tap(now);
                score=rhythm.hits;
                if(rhythm.finished())finishGame();
            }
            return;
        }
        if(page==Page::Catch||page==Page::Training) {
            const bool training=page==Page::Training;
            if(RhythmBack.contains(x,y)){home();return;}
            if(RhythmField.contains(x,y)||RhythmAction.contains(x,y)) {
                if(!(training?strength.begun:stars.begun)){startGame(training,now);return;}
                if(training)strength.tap(now);
                else if(RhythmField.contains(x,y))stars.tap(x,y,now);
                tick(now);
            }
            return;
        }
        if (page!=Page::Home && Back.contains(x,y)) { home(); return; }
        if (page==Page::Home) {
            if (InfoButton.contains(x,y)) { details=!details; return; }
            if (details && ProfileButton.contains(x,y)) { details=false; page=Page::Info; return; }
            if (details) {
                if (!StatusPanel.contains(x,y)) details=false;
                return;
            }
            if (pet.ceremony) return;
            if (PetArea.contains(x,y)) {
                if (pet.isEgg()) pet.eggTap();
                else if (pet.wantEvolveButton()) { decision=Decision::Evolve; page=Page::Confirm; }
                else if (pet.wantFarewellButton()) { decision=Decision::Farewell; page=Page::Confirm; }
                else if (pet.canRunawayNow()) { decision=Decision::Runaway; page=Page::Confirm; }
                else { pet.caress(); message("LOVE!",now,4); }
                return;
            }
            for (int i=0;i<4;++i) if (Nav[i].contains(x,y)) {
                if (pet.isEgg()) { message("Tap the egg to hatch",now); return; }
                if (i==2) { pet.toggleLight(); return; }
                if (pet.sleeping) { message("Wake up first",now); return; }
                if (i==0) page=Page::Food;
                else if (i==1) page=Page::Games;
                else { pet.clean(); message("FRESH!",now,3); }
            }
        } else if (page==Page::Food) {
            for (int i=0;i<4;++i) if (FoodButtons[i].contains(x,y)) {
                if (i<3) pet.feedBerry(i); else pet.feedCandy();
                home(); message("YUM!",now,0);
            }
        } else if (page==Page::Games) {
            if(GameButtons[0].contains(x,y))openRhythm(now);
            for (int i=1;i<3;++i) if (GameButtons[i].contains(x,y)) openGame(i==2,now);
        } else if (page==Page::Info) {
            if (InfoDex.contains(x,y)) page=Page::Dex;
            if (InfoSetup.contains(x,y)) page=Page::Settings;
        } else if (page==Page::Settings) {
            for (int i=0;i<3;++i) if (Rect{12+i*102,50,92,38}.contains(x,y)) {
                brightness=i==0?20:i==1?50:100; settingsDirty=true;
            }
            if (Rect{12,104,142,40}.contains(x,y)) {
                rotated=!rotated; rotateRequested=true; settingsDirty=true;
            }
            if (Rect{166,104,142,40}.contains(x,y)) calibrateRequested=true;
        }
    }
};
}
