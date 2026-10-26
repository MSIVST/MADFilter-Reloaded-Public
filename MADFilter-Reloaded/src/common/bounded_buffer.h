// bounded_buffer.h — Fixed-capacity input buffer for MADFilter Reloaded
//
// Replaces the original's unbounded buffer with a fixed-size ring buffer
// that includes MAD_BUFFER_GUARD zero-padding at the end, as required by
// libmad for frame decoding overflow reads.

#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <new>

namespace madfilter {

// ── Constants ─────────────────────────────────────────────────────────
// Maximum input we'll buffer (1 MiB — per project plan section 9)
constexpr size_t MAD_INPUT_BUFFER_CAPACITY  = 1024 * 1024;
// libmad guard zone: at least MAD_BUFFER_GUARD (8) zero bytes after data
constexpr size_t MAD_BUFFER_GUARD           = 8;
// Total allocation
constexpr size_t MAD_INPUT_BUFFER_ALLOC     = MAD_INPUT_BUFFER_CAPACITY + MAD_BUFFER_GUARD;

class BoundedBuffer {
public:
    BoundedBuffer() {
        m_buffer = new (std::nothrow) uint8_t[MAD_INPUT_BUFFER_ALLOC];
        if (m_buffer) {
            reset();
        }
    }

    ~BoundedBuffer() {
        delete[] m_buffer;
    }

    // Non-copyable, non-movable (ownership is simple)
    BoundedBuffer(const BoundedBuffer&) = delete;
    BoundedBuffer& operator=(const BoundedBuffer&) = delete;

    // ── Reset state ───────────────────────────────────────────────────
    void reset() {
        m_read  = 0;
        m_write = 0;
        // Ensure guard zone is always zero
        if (m_buffer) {
            std::memset(
                m_buffer + MAD_INPUT_BUFFER_CAPACITY,
                0,
                MAD_BUFFER_GUARD);
        }
    }

    // ── How many bytes are available to read? ─────────────────────────
    size_t available() const {
        if (!m_buffer) return 0;
        return m_write - m_read;
    }

    // ── How many bytes of free space remain? ──────────────────────────
    size_t free_space() const {
        return MAD_INPUT_BUFFER_CAPACITY - available();
    }

    // ── Append data to the buffer. Returns bytes actually written. ────
    size_t write(const uint8_t* src, size_t len) {
        if (!m_buffer || !src || len == 0) return 0;
        size_t space = free_space();
        size_t to_write = (std::min)(len, space);

        if (to_write > MAD_INPUT_BUFFER_CAPACITY - m_write) {
            compact();
        }

        // We don't wrap — we compact when full (see compact())
        // Write always appends at m_write
        size_t write_pos = m_write;
        std::memcpy(m_buffer + write_pos, src, to_write);
        m_write += to_write;
        return to_write;
    }

    // ── Compact: move unread data to front of buffer ──────────────────
    void compact() {
        if (!m_buffer) {
            m_read = 0;
            m_write = 0;
            return;
        }
        size_t avail = available();
        if (avail == 0) {
            m_read  = 0;
            m_write = 0;
            return;
        }
        if (m_read > 0) {
            std::memmove(m_buffer, m_buffer + m_read, avail);
            m_write = avail;
            m_read  = 0;
        }
    }

    // ── Get read pointer and count for libmad ─────────────────────────
    uint8_t* read_ptr() {
        return m_buffer ? m_buffer + m_read : nullptr;
    }
    const uint8_t* read_ptr() const {
        return m_buffer ? m_buffer + m_read : nullptr;
    }

    // ── Consume (advance read pointer) ────────────────────────────────
    void consume(size_t n) {
        m_read += (std::min)(n, available());
    }

    // ── Take a span of available data (read-only, does not consume) ───
    struct Span {
        const uint8_t* data;
        size_t         length;
    };
    Span span() const {
        return { read_ptr(), available() };
    }

    // ── Is the buffer empty? ──────────────────────────────────────────
    bool empty() const {
        return m_read >= m_write;
    }

private:
    uint8_t* m_buffer = nullptr;  // allocation of MAD_INPUT_BUFFER_ALLOC bytes
    size_t   m_read   = 0;
    size_t   m_write  = 0;
};

} // namespace madfilter
