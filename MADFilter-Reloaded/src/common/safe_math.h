// safe_math.h — Bounded arithmetic helpers for MADFilter Reloaded
//
// Prevents integer overflow in buffer size calculations.
// Uses MSVC intrinsics when available, with portable fallbacks.

#pragma once
#include <cstdint>
#include <cstddef>
#include <limits>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace madfilter {

// ── Addition with overflow check ──────────────────────────────────────
inline bool safe_add_u32(uint32_t a, uint32_t b, uint32_t* out) {
#ifdef _MSC_VER
    uint32_t result;
    if (_addcarry_u32(0, a, b, &result)) return false;
    *out = result;
    return true;
#else
    if (a > UINT32_MAX - b) return false;
    *out = a + b;
    return true;
#endif
}

inline bool safe_add_sizet(size_t a, size_t b, size_t* out) {
    if (a > SIZE_MAX - b) return false;
    *out = a + b;
    return true;
}

// ── Multiplication with overflow check ────────────────────────────────
inline bool safe_mul_u32(uint32_t a, uint32_t b, uint32_t* out) {
    if (a == 0 || b == 0) { *out = 0; return true; }
    if (a > UINT32_MAX / b) return false;
    *out = a * b;
    return true;
}

inline bool safe_mul_sizet(size_t a, size_t b, size_t* out) {
    if (a == 0 || b == 0) { *out = 0; return true; }
    if (a > SIZE_MAX / b) return false;
    *out = a * b;
    return true;
}

// ── Clamp ─────────────────────────────────────────────────────────────
template<typename T>
inline T clamp(T value, T lo, T hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

} // namespace madfilter
