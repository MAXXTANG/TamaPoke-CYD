#include "../include/ui.h"
#include "../../TamaPoke/embedded_mon.h"
#include "../../TamaPoke/test/shim/Preferences.h"
#include "host_canvas.h"
#include <string>

int main(int argc,char **argv) {
    if (argc!=2) return 2;
    mockNvsReset(); mockSetMillis(5000);
    Pet pet; pet.begin(); pet.selectSpecies(25); pet.fullness=76; pet.joy=88; pet.energy=64; pet.hygiene=92;
    cyd::Model model(pet); cyd::Sprite sprite;
    auto load=[&]() { uint32_t bytes=0; const uint8_t *p=embeddedMonData(pet.speciesId,pet.shiny,bytes); return sprite.load(p,bytes); };
    if (!load()) return 3;
    auto save=[&](const char *name,uint32_t now=5000) {
        CydHostCanvas canvas;
        for (int y=0;y<240;y+=24) { canvas.at(y); cyd::render(canvas,model,sprite,now,false,200000); }
        const std::string path=std::string(argv[1])+"/"+name+".ppm"; canvas.save(path.c_str());
    };
    const cyd::Page pages[]={cyd::Page::Home,cyd::Page::Food,cyd::Page::Games,cyd::Page::Catch,
        cyd::Page::Training,cyd::Page::Result,cyd::Page::Info,cyd::Page::Settings,cyd::Page::Dex,cyd::Page::Confirm};
    const char *names[]={"home","food","games","catch","training","result","info","settings","dex","confirm"};
    for (int i=0;i<10;++i) { model.page=pages[i]; model.score=12; save(names[i]); }
    model.page=cyd::Page::Home; model.details=true; save("status"); model.details=false;
    model.page=cyd::Page::Home; pet.sleeping=true; save("sleep"); pet.sleeping=false;
    pet.shiny=true; if (!load()) return 3; save("shiny");
    pet.selectSpecies(26); if (!load()) return 3; save("raichu");
    pet.newEgg(); save("egg");
    pet.selectSpecies(25); pet.fullness=pet.joy=pet.energy=80; pet.hygiene=100;
    if (!load()) return 3;
    save("approved-home",0);
    model.openRhythm(0);save("rhythm-ready",0);
    model.rhythm.start(0);save("rhythm-wait",500);
    save("rhythm-cue",1100);
    model.rhythm.tap(1100);save("rhythm-perfect",1400);
    model.tick(cyd::Rhythm::Rounds*cyd::Rhythm::BeatMs);save("rhythm-result",18000);
    model.openGame(false,0);save("catch",0);model.startGame(false,0);save("catch-falling",600);
    model.tap(model.stars.targetX,model.stars.targetY(700),700);save("catch-collected",900);
    model.tick(1120);model.tick(4000);save("catch-missed",4100);
    model.tick(20000);save("catch-result",20000);
    model.openGame(true,0);save("training",0);model.startGame(true,0);save("training-charge",850);
    for(uint32_t t:{900u,1800u,2700u})model.tap(200,210,t);
    save("training-punch",2850);model.tap(200,210,3600);save("training-break",3800);
    model.tick(15000);save("training-result",15000);
}
