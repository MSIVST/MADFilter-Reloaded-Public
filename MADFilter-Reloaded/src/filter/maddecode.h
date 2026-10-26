// maddecode.h — Instance-based libmad decoder wrapper
//
// Phase 2: Adds arbitrary-offset frame synchronization, preroll/discard,
// bounded recovery, and explicit sync-state tracking for Fallout 3
// compatibility (radio station changes, seeking, fragmented input).
//
// Original concept by Armin Gerritsen (2002), based on madlld.c by
// Bertrand Petit (2001).  Reworked for MADFilter Reloaded.

#pragma once
#include <cstdint>
#include <cstddef>

extern "C" {
#include "mad.h"
}

namespace madfilter {

// ── Recovery limits (project plan section 9) ─────────────────────────

// Maximum bytes to scan without finding a valid frame header
constexpr size_t MAX_SYNC_SCAN_BYTES      = 512 * 1024;   // 512 KiB

// Maximum consecutive decoder errors before giving up
constexpr int    MAX_CONSECUTIVE_ERRORS    = 128;

// Number of frames to decode-and-discard after first sync
// before outputting PCM (allows Layer III bit reservoir to fill).
// Fallout 3 MP3s are typically CBR 128 kbps stereo; 3–5 preroll
// frames are usually sufficient.
constexpr int    PREROLL_FRAMES            = 5;

// Maximum frames we will ever discard during preroll
constexpr int    MAX_PREROLL_FRAMES        = 32;

// Maximum consecutive bad frames we will conceal before forcing a resync.
// Eight 1152-sample frames is about 209 ms at 44.1 kHz.
constexpr int    MAX_CONCEAL_FRAMES        = 8;

// Maximum number of interleaved int16_t values produced by one MPEG
// audio frame: 1152 samples x 2 channels.  Layer I produces fewer
// (384), but the bound is used to guarantee an output buffer can hold a
// whole frame before decoding starts.
constexpr size_t MAX_PCM_S16_PER_FRAME     = 1152 * 2;

// ── MPEG frame header validation ─────────────────────────────────────

// Returns the frame length in bytes if the 4-byte header at `data` is
// a valid MPEG audio frame header.  Returns 0 if not valid.
//
// Validates: sync word, version, layer, bitrate, sample rate.
// Does NOT validate the CRC or side information.
//
// Frame length formula (ISO 11172-3):
//   Layer I:          (12 * bitrate / samplerate + padding) * 4
//   Layer II:         144 * bitrate / samplerate + padding
//   Layer III MPEG-1: 144 * bitrate / samplerate + padding
//   Layer III MPEG-2/2.5:
//                     72 * bitrate / samplerate + padding
//
uint32_t mpeg_frame_length(const uint8_t data[4]);

// Scan `data`/`len` for a valid MPEG frame header.
// Returns offset of the first valid frame, or SIZE_MAX if none found.
size_t mpeg_find_frame(const uint8_t* data, size_t len);

// ── Decoder sync state ───────────────────────────────────────────────

enum class SyncState {
    LOST,           // No valid frame found yet; scanning
    SYNCING,        // First valid frame found; accumulating preroll
    SYNCHRONIZED,   // Normal decoding; outputting PCM
};

enum class Id3State {
    CHECK,          // Look for an ID3v2 header at the start of the stream
    SKIPPING,       // Discard the remainder of a large ID3v2 tag
    DONE,           // No ID3v2 tag, or the tag has been skipped
};

// ── Decoder struct ───────────────────────────────────────────────────

struct MadDecoder {
    // libmad objects
    mad_stream  stream;
    mad_frame   frame;
    mad_synth   synth;
    mad_timer_t timer;

    // Internal buffer for assembling frames + guard zone
    static constexpr size_t INTERNAL_BUF_SIZE = 8192;
    static constexpr size_t GUARD             = MAD_BUFFER_GUARD;
    uint8_t  ibuf[INTERNAL_BUF_SIZE + GUARD];
    size_t   ibuf_len;

    // ── Sync state (Phase 2) ─────────────────────────────────────────
    SyncState sync_state;
    int       initial_preroll_frames; // policy: 5 generic, 0 BSAudio
    int       preroll_remaining;    // frames left to discard
    int       preroll_frames_decoded; // frames decoded in this SYNCING period
    size_t    bytes_scanned;        // bytes scanned without finding frame
    int       consecutive_errors;   // errors since last good frame
    bool      initialized;
    bool      scan_exhausted;       // gave up on this stream until reset

    // ── ID3v2 tag skipping (stream start only) ───────────────────────
    Id3State  id3_state;
    uint64_t  id3_skip_remaining;   // bytes left in the tag

    // ── Output format (set on first output-capable decode) ───────────
    uint32_t sample_rate;
    uint8_t  channels;
    bool     format_error;         // sticky until a full stream reset

    // ── Concealment state ────────────────────────────────────────────
    mad_timer_t last_frame_duration;
    uint32_t    last_frame_samples;
    uint8_t     last_frame_channels;
    int         concealment_frames;

    // ── Statistics (for diagnostics) ─────────────────────────────────
    uint64_t total_frames_decoded;
    uint64_t total_frames_discarded;
    uint64_t total_sync_attempts;
    uint64_t total_concealed_frames;
    uint64_t total_decode_errors;
    uint32_t last_error;
};

// ── Lifecycle ─────────────────────────────────────────────────────────
void  maddec_init(MadDecoder* d);
void  maddec_finish(MadDecoder* d);
void  maddec_reset(MadDecoder* d);       // full reset (seek/flush)
void  maddec_soft_reset(MadDecoder* d);  // resync without full reinit

// ── Decode one chunk ──────────────────────────────────────────────────
//
// Feeds `in`/`in_len` bytes into the decoder, fills `out` with 16-bit
// stereo-interleaved PCM. A call with a non-null `in` and `in_len == 0`
// drains a complete final frame using libmad's zeroed guard bytes.
//
// Returns: 0 on success, -1 on fatal error, -2 on input exhausted.
//
// On return:
//   *in_consumed  — bytes consumed from input
//   *out_written  — bytes written to output (multiple of sample size)
//   *sync_state    — current sync state after this call
// max_frames bounds successful frame decodes, including discarded preroll.
//
int maddec_decode(MadDecoder* d,
                  const uint8_t* in,  size_t  in_len,
                  int16_t*        out, size_t  out_capacity,
                  size_t*         in_consumed,
                  size_t*         out_written,
                  SyncState*      sync_state,
                  size_t          max_frames = SIZE_MAX);

// Decode already-buffered input without appending new bytes and without
// signalling end-of-stream.  Used to deliver multiple PCM samples from one
// upstream input sample.
int maddec_decode_buffered(MadDecoder* d,
                           int16_t* out, size_t out_capacity,
                           size_t* in_consumed,
                           size_t* out_written,
                           SyncState* sync_state,
                           size_t max_frames = SIZE_MAX);

} // namespace madfilter
