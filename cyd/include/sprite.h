#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cyd {
// A bounded, read-only view of TPK2 data. ESP32 const arrays stay mapped in
// Flash: no ps_malloc, decoded image, or full animation copy is needed.
class Sprite {
public:
    struct Action {
        uint8_t w = 0, h = 0, frames = 0, base = 0, left = 0, right = 0, top = 0;
        const uint8_t *durations = nullptr, *pixels = nullptr;
        uint16_t duration(uint8_t frame) const {
            const uint8_t *p = durations + frame * 2;
            const uint16_t ms = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
            return ms ? ms : 100;
        }
        uint8_t frameAt(uint32_t ms) const {
            uint32_t total = 0;
            for (uint8_t i = 0; i < frames; ++i) total += duration(i);
            if (!total) return 0;
            ms %= total;
            for (uint8_t i = 0; i < frames; ++i) {
                if (ms < duration(i)) return i;
                ms -= duration(i);
            }
            return 0;
        }
    };
    enum : uint8_t { Idle, WalkLeft, WalkRight, Sleep, Eat, Hurt, Attack, Pose,
                     Hop, Nod, Breath, Sit, ActionCount };
    Action actions[ActionCount]{};
    const uint8_t *palette = nullptr;
    uint16_t colors = 0;
    bool loaded = false;

    bool load(const uint8_t *data, size_t size) {
        *this = Sprite();
        if (!data || size < 7 || std::memcmp(data, "TPK2", 4)) return false;
        const uint8_t count = data[4];
        colors = uint16_t(data[5]) | (uint16_t(data[6]) << 8);
        if (!count || count > ActionCount || !colors || colors > 256 ||
            size < size_t(7 + colors * 2)) return false;
        palette = data + 7;
        size_t pos = 7 + colors * 2;
        bool seen[ActionCount]{};
        for (uint8_t i = 0; i < count; ++i) {
            if (size - pos < 4) return fail();
            const uint8_t id = data[pos], w = data[pos+1], h = data[pos+2], n = data[pos+3];
            pos += 4;
            if (id >= ActionCount || seen[id] || !w || !h || !n || n > 24) return fail();
            seen[id] = true;
            const size_t pixels = size_t(w) * h * n;
            const size_t bytes = size_t(n) * 2 + pixels;
            if (bytes > size - pos) return fail();
            Action &a = actions[id];
            a.w = w; a.h = h; a.frames = n; a.durations = data + pos;
            a.pixels = data + pos + n * 2;
            a.left=w; a.top=h;
            for (size_t p = 0; p < pixels; ++p) {
                if (a.pixels[p] == 255) continue;
                if (a.pixels[p] >= colors) return fail();
                const uint8_t col=p%w,row=(p/w)%h;
                if (col<a.left) a.left=col;
                if (col+1>a.right) a.right=col+1;
                if (row<a.top) a.top=row;
                if (row+1>a.base) a.base=row+1;
            }
            if (!a.base) { a.left=a.top=0; a.right=w; }
            pos += bytes;
        }
        if (pos != size || !actions[Idle].frames) return fail();
        loaded = true;
        return true;
    }
    const Action *action(uint8_t id) const {
        if (!loaded) return nullptr;
        return &actions[id < ActionCount && actions[id].frames ? id : uint8_t(Idle)];
    }
    uint16_t color(uint8_t index) const {
        if (index >= colors) return 0;
        const uint8_t *p = palette + index * 2;
        return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }
private:
    bool fail() { *this = Sprite(); return false; }
};
}
