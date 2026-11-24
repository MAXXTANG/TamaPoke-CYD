#include "../../TamaPoke/audio.h"
// v0.1 prototype is deliberately silent; ES8311 is not present on CYD.
// Keep game-core calls link-compatible until the CYD DAC backend is tested.
void audioBegin() {}
void sfxPlay(uint8_t) {}
void audioSetEnabled(bool) {}
bool audioEnabled() { return false; }
void audioSetVolume(uint8_t) {}
uint8_t audioVolume() { return 2; }
void audioSetSleeping(bool) {}
bool audioBusy() { return false; }
void audioSetIdle(bool) {}
