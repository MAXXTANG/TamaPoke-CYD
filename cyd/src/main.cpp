#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include <time.h>
#include "model.h"
#include "input.h"
#include "protocol.h"
#include "touch.h"
#include "ui.h"
#include "../../TamaPoke/embedded_mon.h"

namespace {
constexpr const char *Version=cyd::Version;
#ifdef CYD_ST7789
constexpr const char *Variant="cyd-st7789";
#else
constexpr const char *Variant="cyd-ili9341";
#endif
TFT_eSPI panel;
Pet pet;
cyd::Model model(pet);
cyd::Sprite sprite;
cyd::Touch touch;
cyd::Gesture gesture;
cyd::Calibration calibration;
Preferences settings;
uint32_t lastFrame=0,lastTouch=0,lastClock=0,lastInteract=0;
uint32_t lastFrameUs=0,maxFrameUs=0,renderedFrames=0;
int loadedSpecies=-2;
bool loadedShiny=false,clockSynced=false,renderReady=false;

class Band {
    TFT_eSprite pixels{&panel};
    int top=0;
public:
    bool begin() {
        pixels.setColorDepth(16); pixels.setAttribute(PSRAM_ENABLE,false); pixels.setTextWrap(false);
        return pixels.createSprite(cyd::Width,cyd::BandHeight)!=nullptr;
    }
    void at(int y) { top=y; }
    bool visible(int y,int h) const { return y<top+cyd::BandHeight && y+h>top; }
    void fillScreen(uint16_t c) { pixels.fillSprite(c); }
    void fillRect(int x,int y,int w,int h,uint16_t c) { if (visible(y,h)) pixels.fillRect(x,y-top,w,h,c); }
    void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c) { if (visible(y,h)) pixels.fillRoundRect(x,y-top,w,h,r,c); }
    void drawRoundRect(int x,int y,int w,int h,int r,uint16_t c) { if (visible(y,h)) pixels.drawRoundRect(x,y-top,w,h,r,c); }
    void fillCircle(int x,int y,int r,uint16_t c) { if (visible(y-r,r*2+1)) pixels.fillCircle(x,y-top,r,c); }
    void setCursor(int x,int y) { pixels.setCursor(x,y-top); }
    void setTextSize(int s) { pixels.setTextSize(s); }
    void setTextColor(uint16_t c) { pixels.setTextColor(c); }
    void print(const char *s) { pixels.print(s); }
    void push() { pixels.pushSprite(0,top); }
    void send(Stream &out) {
        uint8_t row[cyd::Width*2];
        for(int y=0;y<cyd::BandHeight;++y) {
            for(int x=0;x<cyd::Width;++x) {
                const uint16_t color=pixels.readPixel(x,y);
                row[x*2]=uint8_t(color);row[x*2+1]=uint8_t(color>>8);
            }
            out.write(row,sizeof(row));
        }
    }
} band;

struct CalibrationSession {
    bool active=false,down=false;
    unsigned step=0,samples=0;
    uint32_t start=0;
    int sumX=0,sumY=0;
    cyd::Point raw[4]{};
} cal;

bool clockValid() { return time(nullptr)>=1735689600; }
void saveSettings() {
    settings.putUChar("brightness",model.brightness);
    settings.putBool("rotated",model.rotated);
    model.settingsDirty=false;
}
void applyBrightness() { ledcWrite(0,uint32_t(model.brightness)*255/100); }
void startCalibration() {
    cal=CalibrationSession(); cal.active=true;
    model.home();
    // Always measure the same physical orientation; restore saved rotation after.
    panel.setRotation(1); gesture.reset();
    Serial.println("CALIBRATE hold each cross for 0.2s, then release; four points");
}
void drawCalibration() {
    panel.fillScreen(cyd::Paper); panel.setTextColor(cyd::Ink); panel.setTextSize(2);
    panel.setCursor(47,80); panel.print("TOUCH CALIBRATION");
    panel.setTextSize(1); panel.setCursor(43,113); panel.print("Hold the cross, then lift your finger");
    char line[32]; snprintf(line,sizeof(line),"Point %u of 4",cal.step+1);
    panel.setCursor(120,136); panel.print(line);
    const int x=cyd::Targets[cal.step].x,y=cyd::Targets[cal.step].y;
    panel.drawCircle(x,y,10,cyd::Red); panel.drawFastHLine(x-14,y,29,cyd::Red);
    panel.drawFastVLine(x,y-14,29,cyd::Red);
}
void calibrationSample(bool held,int x,int y,uint32_t now) {
    if (held) {
        if (!cal.down) { cal.down=true; cal.start=now; cal.samples=0; cal.sumX=cal.sumY=0; }
        if (cal.samples<100) { cal.sumX+=x; cal.sumY+=y; ++cal.samples; }
        return;
    }
    if (!cal.down) return;
    cal.down=false;
    if (now-cal.start<200 || cal.samples<5) return;
    cal.raw[cal.step]={float(cal.sumX)/cal.samples,float(cal.sumY)/cal.samples};
    if (++cal.step<4) { drawCalibration(); return; }
    cyd::Calibration next;
    if (!cyd::fitCalibration(cal.raw,next)) {
        Serial.println("CALIBRATE rejected: inconsistent points; retrying");
        startCalibration(); drawCalibration(); return;
    }
    calibration=next;
    settings.putBytes("touch_v1",&calibration,sizeof(calibration));
    cal.active=false; gesture.reset(); panel.setRotation(model.rotated?3:1);
    model.home(); Serial.println("CALIBRATE saved");
}
void loadSprite() {
    if (pet.speciesId==loadedSpecies && pet.shiny==loadedShiny) return;
    loadedSpecies=pet.speciesId; loadedShiny=pet.shiny;
    uint32_t size=0;
    const uint8_t *data=pet.isEgg()?nullptr:embeddedMonData(pet.speciesId,pet.shiny,size);
    if (!sprite.load(data,size) && !pet.isEgg()) Serial.println("SPRITE unavailable");
    Serial.printf("SPRITE species=%d shiny=%d flash_bytes=%u copied_bytes=0\n",pet.speciesId,pet.shiny,size);
}
void draw(uint32_t now) {
    const uint32_t start=micros();
    const uint32_t heap=ESP.getFreeHeap();
    for (int y=0;y<cyd::Height;y+=cyd::BandHeight) {
        band.at(y); cyd::render(band,model,sprite,now,clockValid(),heap); band.push();
    }
    lastFrameUs=micros()-start;if(lastFrameUs>maxFrameUs)maxFrameUs=lastFrameUs;++renderedFrames;
}
void captureFrame() {
    if(cal.active){Serial.println("ERROR complete touch calibration first");return;}
    const uint32_t now=millis(),heap=ESP.getFreeHeap();const bool valid=clockValid();
    Serial.println("FRAME 320 240 RGB565LE 153600");
    for(int y=0;y<cyd::Height;y+=cyd::BandHeight) {
        band.at(y);cyd::render(band,model,sprite,now,valid,heap);band.push();band.send(Serial);
    }
    Serial.println("DONE");
}
void handleEvent(cyd::Gesture::Event e,uint32_t now) {
    if (e==cyd::Gesture::None) return;
    lastInteract=now;
    if (e==cyd::Gesture::Tap) model.tap(gesture.x,gesture.y,now);
    if (e==cyd::Gesture::Hold && cyd::PetArea.contains(gesture.x,gesture.y)) model.hold(now);
}
void pollTouch(uint32_t now) {
    if (now-lastTouch<20) return;
    lastTouch=now;
    if (touch.held()) {
        int rx,ry;
        if (!touch.raw(rx,ry)) return;
        if (cal.active) { calibrationSample(true,rx,ry,now); return; }
        int x,y;
        if (calibration.map(rx,ry,model.rotated,x,y)) handleEvent(gesture.sample(true,x,y,now),now);
        else if (gesture.down) gesture.moved=true;
    } else if (cal.active) calibrationSample(false,0,0,now);
    else handleEvent(gesture.sample(false,0,0,now),now);
}
void pollBoot(uint32_t now) {
    static bool down=false,longDone=false,calDone=false;
    static uint32_t start=0;
    if (digitalRead(0)==LOW) {
        if (!down) { down=true; start=now; longDone=calDone=false; }
        if (!cal.active && !longDone && now-start>=3000) {
            longDone=true; model.page=cyd::Page::Settings; gesture.consumed=true;
        }
        if (!cal.active && !calDone && now-start>=6000) {
            calDone=true; startCalibration(); drawCalibration();
        }
    } else if (down) {
        down=false;
        if (!longDone && !cal.active && now-start>=35) { model.home(); gesture.reset(); }
    }
}
void status() {
    Serial.printf("STATUS version=%s variant=%s species=%d level=%u food=%u joy=%u energy=%u clean=%u "
                  "heap=%u largest=%u psram=%u band_bytes=%u time_valid=%d calibrated=%d "
                  "frame_us=%u max_frame_us=%u frames=%u uptime_ms=%lu "
                  "page=%u beat=%u hits=%u combo=%u rhythm_running=%d game_running=%d game_score=%u\n",
        Version,Variant,pet.speciesId,pet.level(),pet.fullness,pet.joy,pet.energy,pet.hygiene,
        ESP.getFreeHeap(),heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),ESP.getPsramSize(),
        cyd::Width*cyd::BandHeight*2,clockValid(),calibration.valid(),lastFrameUs,maxFrameUs,renderedFrames,(unsigned long)millis(),
        unsigned(model.page),model.rhythm.beat,model.rhythm.hits,model.rhythm.combo,model.rhythm.running,model.playing(),model.score);
}
void command(const char *line) {
    if (!strcmp(line,"STATUS")) status();
    else if (!strcmp(line,"CAPTURE")) captureFrame();
    else if (!strcmp(line,"SAVE")) { pet.flushSave(); Serial.println("DONE"); }
    else if (!strcmp(line,"TIME")) Serial.printf("TIME %lu valid=%d\n",(unsigned long)time(nullptr),clockValid());
    else if (!strncmp(line,"TIME ",5)) {
        uint32_t epoch=0;
        if (!cyd::parseEpoch(line+5,epoch)) {
            Serial.println("ERROR expected TIME <UTC Unix seconds, year 2025..2099>"); return;
        }
        timeval tv={time_t(epoch),0}; settimeofday(&tv,nullptr);
        if (!clockSynced) { pet.syncClock(epoch); clockSynced=true; }
        else pet.setClock(epoch);
        Serial.printf("TIME %lu valid=1\nDONE\n",(unsigned long)epoch);
    } else if (!strcmp(line,"CALIBRATE")) { startCalibration(); drawCalibration(); }
    else Serial.println("ERROR commands: STATUS, SAVE, TIME [epoch], CALIBRATE, CAPTURE");
}
void pollSerial() {
    static char line[96]; static size_t len=0; static bool overflow=false;
    // Bound work per loop so a noisy serial peer cannot starve touch/rendering.
    for (int n=0;n<128 && Serial.available();++n) {
        const char c=char(Serial.read());
        if (c=='\r') continue;
        if (c=='\n') {
            if (overflow) Serial.println("ERROR line too long");
            else if (len) { line[len]=0; command(line); }
            len=0; overflow=false;
        } else if (len<sizeof(line)-1) line[len++]=c;
        else overflow=true;
    }
}
}

void setup() {
    Serial.begin(115200); delay(150);
    Serial.printf("TamaPoke %s %s: ESP32 / 4MB / no PSRAM\n",Version,Variant);
    pinMode(0,INPUT_PULLUP);
    settings.begin("tamapoke_cyd",false);
    model.brightness=settings.getUChar("brightness",50);
    if (model.brightness!=20 && model.brightness!=50 && model.brightness!=100) model.brightness=50;
    model.rotated=settings.getBool("rotated",false);
    if (settings.isKey("touch_v1") && settings.getBytesLength("touch_v1")==sizeof(calibration))
        settings.getBytes("touch_v1",&calibration,sizeof(calibration));
    panel.init(); panel.setRotation(model.rotated?3:1); panel.setTextWrap(false);
    ledcSetup(0,5000,8); ledcAttachPin(TFT_BL,0); applyBrightness();
    renderReady=band.begin();
    if (!renderReady) {
        panel.fillScreen(TFT_RED); panel.setCursor(12,24); panel.print("Not enough render memory");
        Serial.println("FATAL band allocation failed"); return;
    }
    touch.begin(); randomSeed(esp_random()); pet.begin();
    // First run begins with Pikachu. Subsequent cycles keep the original egg
    // lifecycle, with Pikachu selected as the species supported by this build.
    if (pet.awaitingStarter()) pet.selectSpecies(25);
    else if (pet.isEgg()) pet.chooseStarter(25);
    loadSprite();
    if (!calibration.valid()) { startCalibration(); drawCalibration(); }
    else draw(millis());
    status();
}

void loop() {
    if (!renderReady) { delay(50); return; }
    pollSerial();const uint32_t now=millis();
    pollBoot(now); pollTouch(now);
    const bool wasEgg=pet.isEgg();
    pet.update(now);
    if (!wasEgg && pet.isEgg()) pet.chooseStarter(25);
    model.tick(now);
    if (clockValid() && now-lastClock>=1000) { lastClock=now; pet.lastSeenEpoch=uint32_t(time(nullptr)); }
    // Flash writes can stall for roughly a second: finish the short game
    // round before flushing its deferred periodic save.
    if (pet.savePending() && now-lastInteract>=500 && !model.playing()) pet.flushSave();
    if (model.rotateRequested) {
        model.rotateRequested=false; gesture.reset();
        if (!cal.active) panel.setRotation(model.rotated?3:1);
    }
    if (model.settingsDirty) { saveSettings(); applyBrightness(); }
    if (model.calibrateRequested) {
        model.calibrateRequested=false; startCalibration(); drawCalibration();
    }
    loadSprite();
    if (!cal.active && now-lastFrame>=120) { lastFrame=now; draw(now); }
    delay(1);
}
