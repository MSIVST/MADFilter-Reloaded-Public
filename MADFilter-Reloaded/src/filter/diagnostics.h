// diagnostics.h — Fixed-size, allocation-free event ring for MADFilter
//
// Records state transitions and decoder statistics so an in-game glitch can
// be explained without attaching a debugger.  Writes are constant-time and
// never allocate, so the ring is safe to update from Transform.

#pragma once

#include <cstddef>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace madfilter {

enum DiagCode : uint32_t {
    DIAG_POLICY = 1,
    DIAG_SOURCE_BSA,
    DIAG_SOURCE_GENERIC,
    DIAG_SOURCE_BYTE_CLOCK,
    DIAG_INPUT_FORMAT,
    DIAG_OUTPUT_FORMAT,
    DIAG_SEGMENT,
    DIAG_SEGMENT_RESET,
    DIAG_CONTINUATION,
    DIAG_INPUT_TIMESTAMP,
    DIAG_INPUT_DISCONTINUITY,
    DIAG_FLUSH_BEGIN,
    DIAG_FLUSH_END,
    DIAG_SYNC_ACQUIRED,
    DIAG_SYNC_LOST,
    DIAG_SCAN_EXHAUSTED,
    DIAG_PREROLL_DONE,
    DIAG_DECODED_FORMAT,
    DIAG_DECODE_ERROR,
    DIAG_CONCEAL,
    DIAG_FORMAT_MISMATCH,
    DIAG_CARRY,
    DIAG_CARRY_DRAINED,
    DIAG_UNDERFLOW,
    DIAG_RECEIVE_ERROR,
};

struct DiagEvent {
    uint32_t code;
    uint32_t a;
    uint32_t b;
    uint32_t tick;
};

class DiagRing {
public:
    static constexpr size_t CAPACITY = 1024;

    void clear() {
        m_head = 0;
        m_count = 0;
    }

    void record(uint32_t code, uint32_t a = 0, uint32_t b = 0) {
#ifdef _WIN32
        const uint32_t tick = static_cast<uint32_t>(GetTickCount());
#else
        const uint32_t tick = 0;
#endif
        m_events[m_head] = DiagEvent{code, a, b, tick};
        m_head = (m_head + 1) % CAPACITY;
        if (m_count < CAPACITY) {
            ++m_count;
        }
    }

    size_t size() const {
        return m_count;
    }

    const DiagEvent& at(size_t index) const {
        const size_t start = (m_count == CAPACITY) ? m_head : 0;
        return m_events[(start + index) % CAPACITY];
    }

private:
    DiagEvent m_events[CAPACITY] = {};
    size_t    m_head = 0;
    size_t    m_count = 0;
};

} // namespace madfilter
