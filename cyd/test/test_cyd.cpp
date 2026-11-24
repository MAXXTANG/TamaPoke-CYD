#include "../../TamaPoke/test/framework.h"
#include "../../TamaPoke/test/shim/Preferences.h"
#include "../../TamaPoke/embedded_mon.h"
#include "../include/sprite.h"
#include "../include/model.h"
#include "../include/input.h"
#include "../include/protocol.h"
#include "../include/ui.h"
#include "host_canvas.h"
#include <limits>

TEST(cyd, flash_sprites_and_all_frames_are_bounded) {
    for (int species:{25,26}) for (bool shiny:{false,true}) {
        uint32_t bytes=0; const uint8_t *data=embeddedMonData(species,shiny,bytes);
        cyd::Sprite sprite;
        CHECK(sprite.load(data,bytes)); CHECK(sprite.palette>=data && sprite.palette<data+bytes);
        for (int id=0;id<cyd::Sprite::ActionCount;++id) {
            const auto *a=sprite.action(id); CHECK(a!=nullptr);
            CHECK(a->left<a->right && a->right<=a->w && a->top<a->base && a->base<=a->h);
            CHECK(a->pixels>=data && a->pixels+size_t(a->w)*a->h*a->frames<=data+bytes);
            for (uint32_t t:{0u,1u,1000u,0xffffffffu}) CHECK(a->frameAt(t)<a->frames);
        }
        CHECK(!sprite.load(data,bytes-1)); CHECK(!sprite.loaded);
    }
}
TEST(cyd, usb_time_rejects_overflow_and_invalid_dates) {
    uint32_t epoch=42;
    for (const char *s:{"", "-1", "+1735689600", "1735689600junk", "1735689599", "4102444800",
                       "999999999999999999999999999999999999999999999999999999999999999"}) {
        CHECK(!cyd::parseEpoch(s,epoch)); CHECK_EQ(epoch,42u);
    }
    CHECK(cyd::parseEpoch("1735689600",epoch)); CHECK_EQ(epoch,1735689600u);
    CHECK(cyd::parseEpoch("4102444799",epoch)); CHECK_EQ(epoch,4102444799u);
}
TEST(cyd, malformed_sprite_headers_indices_and_actions) {
    // One one-pixel idle frame, one RGB565 color, 100ms.
    const std::vector<uint8_t> valid={'T','P','K','2',1,1,0,255,255,0,1,1,1,100,0,0};
    cyd::Sprite sprite;
    CHECK(sprite.load(valid.data(),valid.size()));
    for (size_t n=0;n<valid.size();++n) CHECK(!sprite.load(valid.data(),n));
    auto bad=valid; bad.back()=1; CHECK(!sprite.load(bad.data(),bad.size()));
    bad=valid; bad[12]=25; CHECK(!sprite.load(bad.data(),bad.size()));
    bad=valid; bad[9]=12; CHECK(!sprite.load(bad.data(),bad.size()));
    bad=valid; bad[4]=2; bad.insert(bad.end(),valid.begin()+9,valid.end());
    CHECK(!sprite.load(bad.data(),bad.size()));
    bad=valid; bad[13]=bad[14]=0; CHECK(sprite.load(bad.data(),bad.size()));
    CHECK_EQ(sprite.action(0)->duration(0),100);
    CHECK(!sprite.load(nullptr,0));
}
TEST(cyd, four_point_calibration_rotation_and_rejection) {
    cyd::Point raw[4];
    // Deliberately exchanged and reversed sensor axes.
    for (int i=0;i<4;++i) raw[i]={3800-cyd::Targets[i].y*12,300+cyd::Targets[i].x*10};
    cyd::Calibration c;
    CHECK(cyd::fitCalibration(raw,c));
    for (int i=0;i<4;++i) {
        int x,y;
        CHECK(c.map(int(raw[i].x),int(raw[i].y),false,x,y));
        CHECK_EQ(x,int(cyd::Targets[i].x)); CHECK_EQ(y,int(cyd::Targets[i].y));
        CHECK(c.map(int(raw[i].x),int(raw[i].y),true,x,y));
        CHECK_EQ(x,319-int(cyd::Targets[i].x)); CHECK_EQ(y,239-int(cyd::Targets[i].y));
    }
    auto old=c;
    raw[3].x+=400; CHECK(!cyd::fitCalibration(raw,c)); CHECK_EQ(c.a,old.a);
    for (auto &p:raw) p={1000,1000}; CHECK(!cyd::fitCalibration(raw,c));
    raw[0].x=std::numeric_limits<float>::quiet_NaN(); CHECK(!cyd::fitCalibration(raw,c));
    int x,y; CHECK(!c.map(-1,500,false,x,y)); CHECK(!c.map(4096,500,false,x,y));
    c.c=std::numeric_limits<float>::infinity(); CHECK(!c.map(500,500,false,x,y));
}
TEST(cyd, gesture_release_hold_drag_and_millis_wrap) {
    cyd::Gesture g;
    CHECK_EQ(g.sample(true,50,50,0),cyd::Gesture::None);
    CHECK_EQ(g.sample(false,0,0,20),cyd::Gesture::None);
    g.sample(true,50,50,100); CHECK_EQ(g.sample(false,0,0,180),cyd::Gesture::Tap);
    CHECK_EQ(g.sample(false,0,0,200),cyd::Gesture::None);
    g.sample(true,50,50,300); CHECK_EQ(g.sample(true,50,50,3300),cyd::Gesture::Hold);
    CHECK_EQ(g.sample(true,50,50,3400),cyd::Gesture::None);
    CHECK_EQ(g.sample(false,0,0,3500),cyd::Gesture::None);
    g.sample(true,50,50,4000); g.sample(true,80,50,4050);
    CHECK_EQ(g.sample(false,0,0,4200),cyd::Gesture::None);
    g.sample(true,50,50,0xfffffff0); CHECK_EQ(g.sample(false,0,0,0x40),cyd::Gesture::Tap);
}
TEST(cyd, care_actions_persist_without_resetting_the_pet) {
    mockNvsReset(); mockSetMillis(0); Pet p; p.begin(); p.selectSpecies(25);
    p.fullness=20; p.joy=20; p.hygiene=20;
    cyd::Model m(p); m.tap(20,210,100); CHECK_EQ(int(m.page),int(cyd::Page::Food));
    m.tap(30,70,150); CHECK(p.fullness>20);
    m.tap(260,210,200); CHECK_EQ(p.hygiene,100);
    m.tap(180,210,300); CHECK(p.sleeping);
    Pet restored; restored.begin(); CHECK_EQ(restored.speciesId,25); CHECK(restored.sleeping);
    CHECK_EQ(restored.hygiene,100); CHECK_EQ(restored.fullness,p.fullness);
    m.tap(90,210,400); CHECK_EQ(int(m.page),int(cyd::Page::Home));
}
TEST(cyd, catch_game_rewards_once_and_survives_timer_wrap) {
    mockNvsReset(); mockSetMillis(0); Pet p; p.begin(); p.selectSpecies(25);
    cyd::Model m(p); m.startGame(false,0xfffffff0);
    m.tap(m.targetX,m.targetY,0x100); CHECK_EQ(m.score,1);
    m.tap(m.targetX,m.targetY,0x101); CHECK_EQ(m.score,1);
    m.tick(0x5000); CHECK_EQ(int(m.page),int(cyd::Page::Result));
    const auto bond=p.bond; const auto joy=p.joy;
    m.tick(0x9000); m.finishGame(); CHECK_EQ(p.bond,bond); CHECK_EQ(p.joy,joy);
    m.startGame(false,100);m.tick(2400);m.tick(2900);m.tick(5200);m.tick(5700);m.tick(8000);
    CHECK(m.page==cyd::Page::Catch);CHECK_EQ(m.misses,3);
    m.tick(20100);CHECK(m.page==cyd::Page::Result);
}
TEST(cyd, training_exit_and_release_confirmation) {
    mockNvsReset(); mockSetMillis(0); Pet p; p.begin(); p.selectSpecies(25);
    cyd::Model m(p); m.startGame(true,100);
    for (int i=0;i<10;++i) m.tap(150,100,1000+i*900);
    CHECK_EQ(m.score,30); m.tick(15100); CHECK(p.trAtk>0);
    m.home(); m.hold(16000); CHECK_EQ(int(m.page),int(cyd::Page::Confirm));
    m.tap(200,150,16100); CHECK_EQ(p.speciesId,25);
    m.hold(17000); m.tap(50,150,17100); CHECK_EQ(p.ceremony,CER_RELEASE);
}
TEST(cyd, rhythm_timing_window_edges_and_millis_wrap) {
    for(uint32_t start:{100u,0xffffff00u}) {
        for(uint32_t offset:{929u,930u,1380u,1381u}) {
            cyd::Rhythm r;r.start(start);r.tap(start+offset);
            const bool hit=offset>=930&&offset<=1380;
            CHECK_EQ(r.hits,hit?1:0);CHECK_EQ(r.misses,hit?0:1);
            CHECK(r.feedback==(hit?cyd::Rhythm::Perfect:offset<930?cyd::Rhythm::Early:cyd::Rhythm::Late));
            r.tap(start+offset+1);CHECK_EQ(r.hits,hit?1:0);CHECK_EQ(r.misses,hit?0:1);
        }
    }
}
TEST(cyd, rhythm_skipped_beats_break_combo_and_finish_without_double_counting) {
    cyd::Rhythm r;r.start(100);r.tap(1100);r.advance(1600);r.advance(4600);
    CHECK_EQ(r.hits,1);CHECK_EQ(r.misses,2);CHECK_EQ(r.combo,0);CHECK_EQ(r.bestCombo,1);
    r.tap(5600);CHECK_EQ(r.hits,2);CHECK_EQ(r.combo,1);
    r.advance(18100);CHECK(r.finished());CHECK_EQ(r.hits+r.misses,12);
    const auto misses=r.misses;r.advance(99000);r.tap(100000);CHECK_EQ(r.misses,misses);CHECK_EQ(r.hits,2);
    r.start(0xffffff00u);
    for(unsigned beat=0;beat<12;++beat)r.tap(uint32_t(0xffffff00u+beat*1500+1000));
    r.advance(uint32_t(0xffffff00u+18000));
    CHECK(r.finished());CHECK_EQ(r.hits,12);CHECK_EQ(r.bestCombo,12);CHECK_EQ(r.misses,0);
}
TEST(cyd, rhythm_menu_completion_rewards_once_and_cancel_does_not_reward) {
    mockNvsReset();mockSetMillis(0);Pet p;p.begin();p.selectSpecies(25);p.joy=20;
    cyd::Model m(p);m.page=cyd::Page::Games;m.tap(140,60,0);CHECK(m.page==cyd::Page::Rhythm);
    m.tap(200,210,100);CHECK(m.rhythm.running);CHECK_EQ(m.score,0);
    for(unsigned beat=0;beat<12;++beat)m.tap(200,210,100+beat*1500+1000);
    m.tick(18100);CHECK(m.page==cyd::Page::Result);CHECK_EQ(m.score,12);CHECK(m.rhythmResult);CHECK(p.joy>20);
    const auto joy=p.joy,bond=p.bond;m.tick(30000);m.finishGame();CHECK_EQ(p.joy,joy);CHECK_EQ(p.bond,bond);
    m.openRhythm(32000);m.tap(200,210,32100);m.tap(200,210,33100);
    m.tap(40,210,33200);CHECK(m.page==cyd::Page::Home);m.tick(55000);CHECK_EQ(p.joy,joy);CHECK_EQ(p.bond,bond);
    m.openRhythm(56000);m.tap(10,10,56100);m.tap(85,210,56200);CHECK(!m.rhythm.begun);
    m.tap(200,210,56300);m.tick(74300);CHECK(m.page==cyd::Page::Result);CHECK_EQ(m.score,0);
    m.home();m.page=cyd::Page::Games;m.tap(120,108,75000);CHECK(m.page==cyd::Page::Catch);
    m.home();m.page=cyd::Page::Games;m.tap(120,156,76000);CHECK(m.page==cyd::Page::Training);
    m.home();p.sleeping=true;m.openRhythm(77000);CHECK(m.page==cyd::Page::Home);
}
TEST(cyd, rhythm_feedback_frames_match_between_bands_and_full_canvas) {
    mockNvsReset();mockSetMillis(0);Pet p;p.begin();p.selectSpecies(25);cyd::Model m(p);cyd::Sprite sprite;
    uint32_t bytes=0;const auto *data=embeddedMonData(25,false,bytes);CHECK(sprite.load(data,bytes));
    m.openRhythm(0);
    for(uint32_t now:{0u,400u,1100u,1400u,2300u,2900u}) {
        if(now==400)m.rhythm.start(0);
        if(now==1400)m.rhythm.tap(1100);
        if(now==2300)m.rhythm.tap(2300);
        CydHostCanvas full,bands;cyd::render(full,m,sprite,now,false,200000);
        for(int y=0;y<240;y+=24){bands.at(y);cyd::render(bands,m,sprite,now,false,200000);}
        CHECK(full.pixels==bands.pixels);
    }
}
TEST(cyd, settings_and_all_nav_hitboxes_are_on_screen) {
    for (auto r:cyd::Nav) { CHECK(r.x>=0 && r.y>=0); CHECK(r.x+r.w<=320 && r.y+r.h<=240); }
    mockNvsReset(); Pet p; p.begin(); p.selectSpecies(25); cyd::Model m(p);
    m.page=cyd::Page::Settings; m.tap(250,65,100); CHECK_EQ(m.brightness,100); CHECK(m.settingsDirty);
    m.tap(70,122,200); CHECK(m.rotated && m.rotateRequested);
    m.tap(230,122,300); CHECK(m.calibrateRequested);
}
TEST(cyd, stars_fall_inside_touch_field_and_collect_once_per_animation) {
    cyd::StarGame s;s.start(0xffffff00u);const uint32_t hit=uint32_t(0xffffff00u+1000);
    CHECK(s.targetX>=48&&s.targetX<=272);CHECK(s.startY>=72&&s.startY<=88);
    CHECK(s.targetY(hit)>s.startY);CHECK(s.targetY(hit)+28<184);
    CHECK(!s.tap(s.targetX+29,s.targetY(hit),hit));CHECK_EQ(s.score,0);
    CHECK(s.tap(s.targetX+28,s.targetY(hit),hit));CHECK_EQ(s.score,1);CHECK_EQ(s.combo,1);
    CHECK(!s.tap(s.targetX,s.targetY(hit+1),hit+1));CHECK_EQ(s.score,1);
    s.advance(hit+420);CHECK(!s.waiting);CHECK(s.tap(s.targetX,s.targetY(hit+420),hit+420));CHECK_EQ(s.combo,2);
    s.advance(hit+840);s.advance(hit+840+s.lifeMs);CHECK_EQ(s.combo,0);CHECK_EQ(s.misses,1);CHECK(s.running);
    s.advance(uint32_t(0xffffff00u+20000));CHECK(s.finished());
    const auto score=s.score;CHECK(!s.tap(s.targetX,s.targetY(hit),hit+25000));CHECK_EQ(s.score,score);
}
TEST(cyd, strength_charge_grades_cooldown_bag_break_and_recovery) {
    cyd::StrengthGame s;s.start(100);CHECK_EQ(s.charge(100),0u);CHECK(!s.tap(499));
    CHECK(s.tap(500));CHECK_EQ(s.power,1);CHECK_EQ(s.score,1);CHECK_EQ(s.hp,11);
    CHECK(!s.tap(501));CHECK(s.tap(1300));CHECK_EQ(s.power,2);CHECK_EQ(s.score,3);
    for(uint32_t t:{2200u,3100u,4000u}){CHECK(s.tap(t));CHECK_EQ(s.power,3);}
    CHECK_EQ(s.score,12);CHECK_EQ(s.hits,5);CHECK_EQ(s.bags,1);CHECK(s.broken(4400));
    CHECK(!s.tap(4699));CHECK(s.tap(4700));CHECK_EQ(s.power,2);CHECK_EQ(s.hp,10);
    s.advance(15100);CHECK(s.finished());CHECK(!s.tap(15100));CHECK_EQ(s.score,14);
    s.start(0xffffff00u);CHECK(s.tap(uint32_t(0xffffff00u+900)));CHECK_EQ(s.power,3);
    s.advance(uint32_t(0xffffff00u+15000));CHECK(s.finished());
}
TEST(cyd, game_ready_screen_waits_and_early_exit_never_awards) {
    mockNvsReset();mockSetMillis(0);Pet p;p.begin();p.selectSpecies(25);p.joy=20;cyd::Model m(p);
    for(bool training:{false,true}) {
        const auto bond=p.bond,joy=p.joy;m.openGame(training,100);m.tick(30000);
        CHECK(m.page==(training?cyd::Page::Training:cyd::Page::Catch));CHECK(!m.playing());
        m.tap(200,210,30100);CHECK(m.playing());
        if(training)m.tap(200,210,31000);else m.tap(m.stars.targetX,m.stars.targetY(30200),30200);
        m.tap(40,210,31500);m.tick(60000);CHECK(m.page==cyd::Page::Home);CHECK_EQ(p.bond,bond);CHECK_EQ(p.joy,joy);
    }
    m.startGame(true,0);m.tap(200,210,900);m.tick(15000);CHECK(m.page==cyd::Page::Result);
    const auto bond=p.bond;const auto atk=p.trAtk;m.tick(18000);m.finishGame();CHECK_EQ(p.bond,bond);CHECK_EQ(p.trAtk,atk);
    m.openGame(true,20000);p.sleeping=true;m.startGame(true,21000);CHECK(!m.playing());
}
TEST(cyd, game_impact_and_miss_visuals_match_full_and_band_rendering) {
    mockNvsReset();mockSetMillis(0);Pet p;p.begin();p.selectSpecies(25);cyd::Model m(p);cyd::Sprite sprite;
    uint32_t bytes=0;const auto *data=embeddedMonData(25,false,bytes);CHECK(sprite.load(data,bytes));
    auto compare=[&](uint32_t now){CydHostCanvas full,bands;cyd::render(full,m,sprite,now,false,200000);
        for(int y=0;y<240;y+=24){bands.at(y);cyd::render(bands,m,sprite,now,false,200000);}CHECK(full.pixels==bands.pixels);};
    m.openGame(false,0);compare(0);m.startGame(false,0);compare(500);
    m.tap(m.stars.targetX,m.stars.targetY(600),600);compare(750);m.tick(1020);m.tick(4000);compare(4100);
    m.openGame(true,0);compare(0);m.startGame(true,0);compare(600);
    for(uint32_t t:{900u,1800u,2700u,3600u}){m.tap(200,210,t);compare(t+150);}
    CHECK_EQ(m.strength.bags,1);compare(4100);
}
TEST(cyd, pixel_home_targets_and_status_overlay_do_not_leak_taps) {
    mockNvsReset(); mockSetMillis(0); Pet p; p.begin(); p.selectSpecies(25);
    cyd::Model m(p); p.hygiene=20;
    // The visible gaps and area under the pet must not trigger care actions.
    for (int x:{0,7,72,80,87,152,160,167,232,240,247,312,319}) {
        m.tap(x,210,100); CHECK(m.page==cyd::Page::Home); CHECK_EQ(p.hygiene,20);
    }
    for (int x:{10,70}) { m.tap(x,230,200); CHECK(m.page==cyd::Page::Food); m.home(); }
    m.tap(90,230,300); CHECK(m.page==cyd::Page::Games); m.home();
    m.tap(170,230,400); CHECK(p.sleeping);
    m.tap(230,230,500); CHECK(!p.sleeping);
    m.tap(310,230,600); CHECK_EQ(p.hygiene,100);
    m.tap(300,24,700); CHECK(m.details);
    const auto bond=p.bond; m.tap(150,112,800); CHECK(m.details); CHECK_EQ(p.bond,bond);
    p.hygiene=20; m.tap(280,210,900); CHECK(!m.details); CHECK_EQ(p.hygiene,20);
    m.tap(300,24,1000); m.tap(230,54,1100); CHECK(m.page==cyd::Page::Info); CHECK(!m.details);
    m.tap(240,210,1200); CHECK(m.page==cyd::Page::Settings);
    m.home(); m.tap(300,24,1300); m.hold(4400); CHECK(!m.details); CHECK(m.page==cyd::Page::Home);
    CHECK_EQ(p.speciesId,25);
}
TEST(cyd, pixel_character_keeps_native_scale_and_foot_alignment) {
    cyd::Sprite sprite; uint32_t bytes=0;
    const uint8_t *data=embeddedMonData(25,false,bytes); CHECK(sprite.load(data,bytes));
    const auto *idle=sprite.action(cyd::Sprite::Idle); CHECK(idle!=nullptr);
    const auto bounds=cyd::frameBounds(*idle,0);
    CHECK(bounds.valid); CHECK_EQ(bounds.right-bounds.left,19); CHECK_EQ(bounds.bottom-bounds.top,25);
    CydHostCanvas canvas; canvas.fillScreen(0xf81f); cyd::pixelPet(canvas,sprite,cyd::Sprite::Idle,0);
    int left=320,top=240,right=0,bottom=0;
    for(int y=0;y<240;++y)for(int x=0;x<320;++x)if(canvas.pixels[y*320+x]!=0xf81f) {
        if(x<left)left=x;
        if(y<top)top=y;
        if(x+1>right)right=x+1;
        if(y+1>bottom)bottom=y+1;
    }
    CHECK_EQ(left,120); CHECK_EQ(right,196); CHECK_EQ(top,72); CHECK_EQ(bottom,172);
    // Every native sprite pixel expands to a uniform 4x4 square.
    for(int y=top;y<bottom;y+=4)for(int x=left;x<right;x+=4)
        for(int dy=0;dy<4;++dy)for(int dx=0;dx<4;++dx)
            CHECK_EQ(canvas.pixels[(y+dy)*320+x+dx],canvas.pixels[y*320+x]);
}
TEST(cyd, band_render_matches_full_render_for_all_pages) {
    mockNvsReset(); mockSetMillis(5000); Pet p; p.begin(); p.selectSpecies(25);
    cyd::Model m(p); cyd::Sprite sprite; uint32_t bytes=0;
    const uint8_t *data=embeddedMonData(25,false,bytes); CHECK(sprite.load(data,bytes));
    for (auto page:{cyd::Page::Home,cyd::Page::Food,cyd::Page::Games,cyd::Page::Catch,
        cyd::Page::Training,cyd::Page::Result,cyd::Page::Info,cyd::Page::Settings,cyd::Page::Dex,cyd::Page::Confirm}) {
        m.page=page; m.gameStart=0;
        CydHostCanvas full,bands;
        cyd::render(full,m,sprite,5000,false,200000);
        for (int top=0;top<240;top+=24) { bands.at(top); cyd::render(bands,m,sprite,5000,false,200000); }
        CHECK(full.pixels==bands.pixels);
    }
    m.page=cyd::Page::Home;
    for (bool night:{false,true}) for (bool details:{false,true}) {
        p.sleeping=night; m.details=details;
        CydHostCanvas full,bands;
        cyd::render(full,m,sprite,5000,false,200000);
        for (int top=0;top<240;top+=24) { bands.at(top); cyd::render(bands,m,sprite,5000,false,200000); }
        CHECK(full.pixels==bands.pixels);
    }
    CHECK(sizeof(cyd::Sprite)<1024); CHECK_EQ(cyd::Width*cyd::BandHeight*2,15360);
}
