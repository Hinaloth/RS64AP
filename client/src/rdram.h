// RDRAM access by KSEG0 address. The recomp keeps RDRAM word-swapped: byte n of a big-endian word is at
// (addr ^ 3), as N64Recomp's MEM_B does.
#pragma once
#include <cstdint>
#include <cstring>

namespace rs64ap {

inline uint8_t rb(const uint8_t* rdram, uint32_t a) { return rdram[(a - 0x80000000u) ^ 3]; }
inline void wb(uint8_t* rdram, uint32_t a, uint8_t v) { rdram[(a - 0x80000000u) ^ 3] = v; }

inline uint32_t rw(const uint8_t* rdram, uint32_t a) {
    uint32_t v = 0;
    for (uint32_t i = 0; i < 4; ++i) v = (v << 8) | rb(rdram, a + i);
    return v;
}
inline void ww(uint8_t* rdram, uint32_t a, uint32_t v) {
    for (uint32_t i = 0; i < 4; ++i) wb(rdram, a + i, (uint8_t)(v >> (24 - 8 * i)));
}

inline float rf(const uint8_t* rdram, uint32_t a) {
    const uint32_t b = rw(rdram, a);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
inline void wf(uint8_t* rdram, uint32_t a, float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    ww(rdram, a, b);
}
inline uint32_t float_bits(float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}

// The prefix of librecomp's recomp_context: the 32 general-purpose registers. Enough for hooks and exports.
struct Regs {
    uint64_t r[32];
};
inline Regs* regs(void* ctx) { return static_cast<Regs*>(ctx); }

// MIPS register numbers used by hook contracts.
enum : int { kV0 = 2, kV1 = 3, kA0 = 4, kA1 = 5, kA2 = 6, kA3 = 7, kS0 = 16, kS1 = 17 };

}  // namespace rs64ap
