#pragma once
#include <cstdint>

namespace cyd {
// One release per beat. Unsigned elapsed time also works across millis wrap.
struct Rhythm {
    static constexpr uint32_t BeatMs=1500,OpenMs=930,CloseMs=1380,FeedbackMs=800;
    static constexpr uint8_t Rounds=12;
    enum Feedback : uint8_t { None, Perfect, Early, Late, Miss };
    bool begun=false,running=false,attempted=false;
    uint8_t beat=0,hits=0,combo=0,bestCombo=0,misses=0;
    uint32_t startedAt=0,feedbackAt=0;
    Feedback feedback=None;
    void reset() { *this=Rhythm(); }
    void start(uint32_t now) { reset();begun=running=true;startedAt=now; }
    bool finished() const { return begun&&!running; }
    uint32_t elapsed(uint32_t now) const { return begun?now-startedAt:0; }
    uint32_t phase(uint32_t now) const { return elapsed(now)%BeatMs; }
    bool window(uint32_t now) const { return running&&phase(now)>=OpenMs&&phase(now)<=CloseMs; }
    bool showingFeedback(uint32_t now) const { return feedback!=None&&now-feedbackAt<FeedbackMs; }
    void advance(uint32_t now) {
        if(!running)return;
        const uint32_t index=elapsed(now)/BeatMs;
        const uint8_t next=index>=Rounds?Rounds:uint8_t(index);
        if(next>beat) {
            const uint8_t missed=uint8_t(next-beat-1+(!attempted?1:0));
            if(missed) { misses+=missed;combo=0;feedback=Miss;feedbackAt=now; }
            beat=next;attempted=false;
        }
        if(beat==Rounds)running=false;
    }
    void tap(uint32_t now) {
        advance(now);
        if(!running||attempted)return;
        attempted=true;feedbackAt=now;
        if(window(now)) {
            feedback=Perfect;++hits;++combo;
            if(combo>bestCombo)bestCombo=combo;
        } else { feedback=phase(now)<OpenMs?Early:Late;combo=0;++misses; }
    }
};
}
