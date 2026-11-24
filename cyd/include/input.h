#pragma once
#include <cstdint>
#include <cstdlib>

namespace cyd {
// One action per physical contact. A hold consumes the contact, and moving
// between controls never turns into a tap on release.
struct Gesture {
    enum Event { None, Tap, Hold };
    bool down=false,moved=false,consumed=false;
    int x=0,y=0;
    uint32_t started=0;
    Event sample(bool pressed,int px,int py,uint32_t now) {
        if (pressed) {
            if (!down) { down=true; moved=false; consumed=false; x=px; y=py; started=now; }
            if (std::abs(px-x)>18 || std::abs(py-y)>18) moved=true;
            if (!moved && !consumed && now-started>=3000) { consumed=true; return Hold; }
            return None;
        }
        if (!down) return None;
        down=false;
        return !moved && !consumed && now-started>=35?Tap:None;
    }
    void reset() { *this=Gesture(); }
};
}
