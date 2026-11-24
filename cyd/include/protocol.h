#pragma once
#include <cstdint>

namespace cyd {
inline bool parseEpoch(const char *text,uint32_t &epoch) {
    if (!text || !*text) return false;
    uint64_t value=0;
    for (const char *p=text;*p;++p) {
        if (*p<'0' || *p>'9') return false;
        value=value*10+uint8_t(*p-'0');
        // Bounded at each step, including arbitrarily long input.
        if (value>4102444799ULL) return false;
    }
    if (value<1735689600ULL) return false;
    epoch=uint32_t(value);
    return true;
}
}
