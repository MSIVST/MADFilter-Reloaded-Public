// maddecode.cpp — Instance-based libmad decoder wrapper (Phase 2)
//
// Adds arbitrary-offset frame synchronization, preroll/discard,
// and bounded recovery for Fallout 3 radio compatibility.
//
// Based on the original MADFilter maddecode.cpp by Armin Gerritsen (2002),
// which was in turn based on madlld.c by Bertrand Petit (2001).

#include "maddecode.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace madfilter {

// ═══════════════════════════════════════════════════════════════════════
// MPEG frame header validation
// ═══════════════════════════════════════════════════════════════════════

// ── Bitrate table (ISO 11172-3) ──────────────────────────────────────
// Indexed by [version_idx][layer_idx][bitrate_index]
// version_idx: 0=MPEG2.5, 1=reserved, 2=MPEG2, 3=MPEG1
// layer_idx:   0=reserved, 1=LayerIII, 2=LayerII, 3=LayerI

// Bitrate table: [version_idx][layer_idx][bitrate_index]
// bitrate_index 0 = free (rejected), 15 = forbidden (rejected)
// Values are in kbps.
// Source: ISO/IEC 11172-3
static const uint16_t bitrate_table[4][4][16] = {
    // MPEG 2.5 — same bitrates as MPEG 2
    {{0},
     {0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160, 0}, // L3
     {0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160, 0}, // L2
     {0, 32, 48, 56, 64, 80, 96,112,128,144,160,176,192,224,256, 0}},// L1
    // reserved
    {{0},{0},{0},{0}},
    // MPEG 2
    {{0},
     {0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160, 0}, // L3
     {0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160, 0}, // L2
     {0, 32, 48, 56, 64, 80, 96,112,128,144,160,176,192,224,256, 0}},// L1
    // MPEG 1
    {{0},
     {0, 32, 40, 48, 56, 64, 80, 96,112,128,160,192,224,256,320, 0}, // L3
     {0, 32, 48, 56, 64, 80, 96,112,128,160,192,224,256,320,384, 0}, // L2
     {0, 32, 64, 96,128,160,192,224,256,288,320,352,384,416,448, 0}} // L1
};

// ── Sample rate table ─────────────────────────────────────────────────
// Indexed by [version_idx][sample_rate_index]

static const uint16_t sample_rate_table[4][4] = {
    {11025, 12000,  8000, 0},  // MPEG 2.5
    {    0,     0,     0, 0},  // reserved
    {22050, 24000, 16000, 0},  // MPEG 2
    {44100, 48000, 32000, 0},  // MPEG 1
};

uint32_t mpeg_frame_length(const uint8_t data[4]) {
    // Must have sync word: 0xFFE0
    if (data[0] != 0xFF || (data[1] & 0xE0) != 0xE0)
        return 0;

    // Parse header fields
    unsigned int version_idx = (data[1] >> 3) & 0x03;   // 2 bits
    unsigned int layer_idx   = (data[1] >> 1) & 0x03;   // 2 bits
    // unsigned int protection  = data[1] & 0x01;  // reserved for future CRC validation
    unsigned int bitrate_idx = (data[2] >> 4) & 0x0F;   // 4 bits
    unsigned int samplerate_idx = (data[2] >> 2) & 0x03; // 2 bits
    unsigned int padding     = (data[2] >> 1) & 0x01;

    // Reject invalid combinations
    if (version_idx == 1)  return 0;  // reserved version
    if (layer_idx == 0)    return 0;  // reserved layer
    if (bitrate_idx == 0)  return 0;  // "free" format — reject
    if (bitrate_idx == 15) return 0;  // reserved/forbidden
    if (samplerate_idx == 3) return 0; // reserved

    uint16_t bitrate    = bitrate_table[version_idx][layer_idx][bitrate_idx];
    uint16_t samplerate = sample_rate_table[version_idx][samplerate_idx];

    if (bitrate == 0 || samplerate == 0)
        return 0;

    // Calculate frame length
    uint32_t frame_bytes;
    if (layer_idx == 3) {
        // Layer I
        frame_bytes = (12 * bitrate * 1000 / samplerate + padding) * 4;
    } else if (layer_idx == 1 && version_idx != 3) {
        // Layer III, MPEG-2 or MPEG-2.5
        frame_bytes = 72 * bitrate * 1000 / samplerate + padding;
    } else {
        // Layer II, or Layer III MPEG-1
        frame_bytes = 144 * bitrate * 1000 / samplerate + padding;
    }

    // Sanity check frame length
    if (frame_bytes < 21 || frame_bytes > 5760)
        return 0;

    // For free-format or certain edge cases, reject unreasonable sizes
    // But standard frames should be 24–2881 bytes for Layer II/III,
    // 192–5760 for Layer I

    return frame_bytes;
}

// Two frame headers belong to the same MPEG stream when version, layer,
// and sample rate match.  Bitrate may change (VBR), and channel mode may
// change per frame.
static bool mpeg_headers_compatible(const uint8_t first[4],
                                    const uint8_t second[4]) {
    const unsigned first_version  = (first[1]  >> 3) & 0x03;
    const unsigned second_version = (second[1] >> 3) & 0x03;
    const unsigned first_layer    = (first[1]  >> 1) & 0x03;
    const unsigned second_layer   = (second[1] >> 1) & 0x03;
    const unsigned first_rate     = (first[2]  >> 2) & 0x03;
    const unsigned second_rate    = (second[2] >> 2) & 0x03;

    return first_version == second_version &&
           first_layer   == second_layer   &&
           first_rate    == second_rate;
}

// Detect a Layer III Xing/Info/VBRI metadata frame.  These frames carry
// seek tables rather than audio, so they must not consume an audio
// preroll slot.
static bool mpeg_frame_is_metadata(const uint8_t* frame, size_t len) {
    if (!frame || len < 4) return false;

    const unsigned version_idx = (frame[1] >> 3) & 0x03;
    const unsigned layer_idx   = (frame[1] >> 1) & 0x03;
    if (layer_idx != 1 || version_idx == 1) return false;  // Layer III only

    const bool mono = ((frame[3] >> 6) & 0x03) == 3;
    const size_t side_info =
        (version_idx == 3) ? (mono ? 17 : 32) : (mono ? 9 : 17);
    const size_t crc = (frame[1] & 0x01) ? 0 : 2;
    const size_t offset = 4 + crc + side_info;

    if (len >= offset + 4) {
        const uint8_t* tag = frame + offset;
        if (std::memcmp(tag, "Xing", 4) == 0 ||
            std::memcmp(tag, "Info", 4) == 0) {
            return true;
        }
    }

    // VBRI is stored 32 bytes after the frame header.
    if (version_idx == 3 && len >= 36 + 4 &&
        std::memcmp(frame + 36, "VBRI", 4) == 0) {
        return true;
    }

    return false;
}

size_t mpeg_find_frame(const uint8_t* data, size_t len) {
    if (!data || len < 4) return SIZE_MAX;

    // Only scan the first MAX_SYNC_SCAN_BYTES
    size_t scan_end = len;
    if (scan_end > MAX_SYNC_SCAN_BYTES) scan_end = MAX_SYNC_SCAN_BYTES;

    for (size_t i = 0; i <= scan_end - 4; i++) {
        if (data[i] != 0xFF) continue;

        uint32_t frame_len = mpeg_frame_length(&data[i]);
        if (frame_len == 0) continue;

        // The complete first frame must fit in the buffer.
        if (frame_len > len - i) continue;

        // Validate that a second compatible frame follows.  This reduces
        // false positives significantly.
        const size_t next_pos = i + frame_len;
        if (len - next_pos >= 4) {
            if (data[next_pos] == 0xFF &&
                (data[next_pos + 1] & 0xE0) == 0xE0 &&
                mpeg_frame_length(&data[next_pos]) > 0 &&
                mpeg_headers_compatible(&data[i], &data[next_pos]))
            {
                return i;
            }
            continue;
        }

        // If there are not enough bytes left to validate a second header,
        // accept the first frame tentatively when it ends within the
        // buffer.  A zero-length drain or the next input chunk will
        // confirm it normally.
        if (next_pos <= len) {
            return i;
        }
    }

    return SIZE_MAX;
}

// ═══════════════════════════════════════════════════════════════════════
// Fixed-point → 16-bit PCM conversion
// ═══════════════════════════════════════════════════════════════════════

static inline int16_t mad_fixed_to_s16(mad_fixed_t sample) {
    sample += (1L << (MAD_F_FRACBITS - 16));
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;
    return static_cast<int16_t>(sample >> (MAD_F_FRACBITS + 1 - 16));
}

// ═══════════════════════════════════════════════════════════════════════
// Decoder lifecycle
// ═══════════════════════════════════════════════════════════════════════

void maddec_init(MadDecoder* d) {
    if (!d) return;

    mad_stream_init(&d->stream);
    mad_frame_init(&d->frame);
    mad_synth_init(&d->synth);
    mad_timer_reset(&d->timer);

    std::memset(d->ibuf, 0, sizeof(d->ibuf));
    d->ibuf_len    = 0;
    d->initialized = true;
    d->sync_state  = SyncState::LOST;
    d->initial_preroll_frames = PREROLL_FRAMES;
    d->preroll_remaining = 0;
    d->preroll_frames_decoded = 0;
    d->bytes_scanned = 0;
    d->consecutive_errors = 0;
    d->scan_exhausted = false;
    d->id3_state = Id3State::CHECK;
    d->id3_skip_remaining = 0;
    d->sample_rate = 0;
    d->channels    = 0;
    d->format_error = false;
    mad_timer_reset(&d->last_frame_duration);
    d->last_frame_samples    = 0;
    d->last_frame_channels   = 0;
    d->concealment_frames    = 0;

    d->total_frames_decoded  = 0;
    d->total_frames_discarded = 0;
    d->total_sync_attempts   = 0;
    d->total_concealed_frames = 0;
    d->total_decode_errors   = 0;
    d->last_error            = 0;
}

void maddec_finish(MadDecoder* d) {
    if (!d || !d->initialized) return;

    mad_synth_finish(&d->synth);
    mad_frame_finish(&d->frame);
    mad_stream_finish(&d->stream);
    d->initialized = false;
}

void maddec_reset(MadDecoder* d) {
    if (!d) return;

    // Full reinit — used on flush/NewSegment
    mad_synth_finish(&d->synth);
    mad_frame_finish(&d->frame);
    mad_stream_finish(&d->stream);

    mad_stream_init(&d->stream);
    mad_frame_init(&d->frame);
    mad_synth_init(&d->synth);
    mad_timer_reset(&d->timer);

    std::memset(d->ibuf, 0, sizeof(d->ibuf));
    d->ibuf_len    = 0;
    d->sync_state  = SyncState::LOST;
    d->preroll_remaining = 0;
    d->preroll_frames_decoded = 0;
    d->bytes_scanned = 0;
    d->consecutive_errors = 0;
    d->scan_exhausted = false;
    d->id3_state = Id3State::CHECK;
    d->id3_skip_remaining = 0;
    mad_timer_reset(&d->last_frame_duration);
    d->last_frame_samples  = 0;
    d->last_frame_channels = 0;
    d->concealment_frames  = 0;
    // A new stream may have a different format on the same filter instance.
    d->sample_rate = 0;
    d->channels = 0;
    d->format_error = false;
}

void maddec_soft_reset(MadDecoder* d) {
    if (!d) return;

    // Soft reset — keep libmad objects but reset sync tracking
    // Used when the stream continues but we lost sync temporarily.
    // We compact the internal buffer and restart frame search.
    mad_frame_mute(&d->frame);
    mad_synth_mute(&d->synth);
    d->sync_state          = SyncState::LOST;
    d->preroll_remaining   = 0;
    d->preroll_frames_decoded = 0;
    d->bytes_scanned       = 0;
    d->consecutive_errors  = 0;
    d->scan_exhausted      = false;
    // A soft resync happens mid-stream; do not look for another ID3 tag.
    d->id3_state           = Id3State::DONE;
    d->id3_skip_remaining  = 0;
    d->concealment_frames  = 0;
}

// ═══════════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════════

static void feed_stream(MadDecoder* d, bool end_of_stream) {
    if (d->ibuf_len > 0) {
        size_t buffer_len = d->ibuf_len;
        if (end_of_stream) {
            buffer_len += d->GUARD;
        }
        mad_stream_buffer(&d->stream, d->ibuf, buffer_len);
        d->stream.error = MAD_ERROR_NONE;
    }
}

static size_t compact_ibuf(MadDecoder* d) {
    if (d->stream.next_frame) {
        size_t remaining = d->ibuf_len - (d->stream.next_frame - d->ibuf);
        if (remaining > 0 && remaining <= d->ibuf_len) {
            std::memmove(d->ibuf, d->stream.next_frame, remaining);
            d->ibuf_len = remaining;

            // Rebase libmad's buffer pointers so a second compaction before
            // the next decode is idempotent.  Without this, next_frame still
            // points at the pre-move offset and valid buffered bytes are lost.
            mad_stream_buffer(&d->stream, d->ibuf, d->ibuf_len);
            d->stream.error = MAD_ERROR_NONE;
            return remaining;
        }
    }
    d->ibuf_len = 0;
    d->stream.buffer = nullptr;
    d->stream.bufend = nullptr;
    d->stream.this_frame = nullptr;
    d->stream.next_frame = nullptr;
    return 0;
}

// Write PCM samples from a synthesized frame to the output buffer.
// Returns number of int16_t samples written (not bytes).
static size_t write_pcm(const mad_synth* synth, int16_t* out, size_t max_frames) {
    unsigned int nsamples  = synth->pcm.length;
    unsigned int nchannels = synth->pcm.channels;

    if (nchannels < 1 || nchannels > 2) return 0;

    size_t needed = static_cast<size_t>(nsamples) * nchannels;
    if (needed > max_frames) return 0;

    const mad_fixed_t* left  = synth->pcm.samples[0];
    const mad_fixed_t* right = (nchannels == 2) ? synth->pcm.samples[1] : nullptr;

    size_t written = 0;
    for (unsigned int i = 0; i < nsamples; i++) {
        out[written++] = mad_fixed_to_s16(left[i]);
        if (nchannels == 2 && right) {
            out[written++] = mad_fixed_to_s16(right[i]);
        }
    }
    return written;
}

// Emit one frame of silence so a bad frame costs quality, not timeline.
static bool write_concealment(MadDecoder* d, int16_t* out,
                              size_t max_out_frames, size_t* total_out) {
    if (d->last_frame_samples == 0 ||
        d->last_frame_channels == 0 ||
        d->concealment_frames >= MAX_CONCEAL_FRAMES) {
        return false;
    }

    const size_t needed = static_cast<size_t>(d->last_frame_samples) *
                          d->last_frame_channels;
    if (needed > max_out_frames - *total_out) {
        return false;
    }

    std::memset(out + *total_out, 0, needed * sizeof(int16_t));
    *total_out += needed;
    mad_timer_add(&d->timer, d->last_frame_duration);
    d->concealment_frames++;
    d->total_concealed_frames++;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// Main decode function
// ═══════════════════════════════════════════════════════════════════════

static int maddec_decode_internal(MadDecoder* d,
                                  const uint8_t* in,  size_t  in_len,
                                  bool end_of_stream,
                                  int16_t*        out, size_t  out_capacity,
                                  size_t*         in_consumed,
                                  size_t*         out_written,
                                  SyncState*      sync_state,
                                  size_t          max_frames)
{
    if (!d || !d->initialized || (!in && in_len != 0) ||
        !out || !in_consumed || !out_written) {
        if (in_consumed)  *in_consumed  = 0;
        if (out_written)  *out_written  = 0;
        if (sync_state && d) *sync_state = d->sync_state;
        return -1;
    }

    *in_consumed  = 0;
    *out_written  = 0;
    if (d->format_error) {
        if (sync_state) *sync_state = d->sync_state;
        return -1;
    }
    size_t max_out_frames = out_capacity / sizeof(int16_t);
    size_t total_out = 0;
    const uint64_t start_frames = d->total_frames_decoded;

    // ── Skip the remainder of a large ID3v2 tag ──────────────────────
    const uint8_t* input = in;
    size_t input_len = in_len;
    size_t skipped = 0;

    if (d->id3_skip_remaining > 0) {
        const uint64_t available = static_cast<uint64_t>(input_len);
        const uint64_t skip = (available < d->id3_skip_remaining)
                                  ? available
                                  : d->id3_skip_remaining;
        input += static_cast<size_t>(skip);
        input_len -= static_cast<size_t>(skip);
        d->id3_skip_remaining -= skip;
        skipped = static_cast<size_t>(skip);
        *in_consumed = skipped;

        if (d->id3_skip_remaining > 0) {
            if (sync_state) *sync_state = d->sync_state;
            return -2;  // tag still spans future input
        }
        d->id3_state = Id3State::DONE;
    }

    // ── Append new input to internal buffer ──────────────────────────
    size_t append_len = input_len;
    size_t space = sizeof(d->ibuf) - d->GUARD - d->ibuf_len;
    if (append_len > space) {
        // Compact first, then try again
        // Before sync acquisition libmad has no next_frame pointer. Keep
        // that unresolved input; the scanner below releases scanned bytes.
        if (d->sync_state != SyncState::LOST) compact_ibuf(d);
        space = sizeof(d->ibuf) - d->GUARD - d->ibuf_len;
        if (append_len > space) {
            append_len = space;
        }
    }

    if (append_len > 0) {
        std::memcpy(d->ibuf + d->ibuf_len, input, append_len);
        d->ibuf_len += append_len;
        *in_consumed = skipped + append_len;
    }

    // Zero the guard zone
    std::memset(d->ibuf + d->ibuf_len, 0, d->GUARD);

    // ── ID3v2 detection at the start of the stream ───────────────────
    if (d->id3_state == Id3State::CHECK) {
        if (d->ibuf_len < 3) {
            if (sync_state) *sync_state = d->sync_state;
            return -2;
        }

        if (d->ibuf[0] != 'I' || d->ibuf[1] != 'D' || d->ibuf[2] != '3') {
            d->id3_state = Id3State::DONE;
        } else {
            if (d->ibuf_len < 10) {
                if (sync_state) *sync_state = d->sync_state;
                return -2;  // wait for the complete tag header
            }

            // ID3v2 size is four syncsafe bytes (7 bits each).
            bool valid_size = true;
            uint32_t tag_payload = 0;
            for (size_t i = 6; i < 10; ++i) {
                if ((d->ibuf[i] & 0x80) != 0) {
                    valid_size = false;
                    break;
                }
                tag_payload = (tag_payload << 7) |
                              static_cast<uint32_t>(d->ibuf[i] & 0x7F);
            }

            if (!valid_size) {
                // Not a well-formed tag; treat the bytes as stream data.
                d->id3_state = Id3State::DONE;
            } else {
                const uint8_t flags = d->ibuf[5];
                const uint64_t total =
                    10ull + tag_payload +
                    ((flags & 0x10) != 0 ? 10ull : 0ull);  // v2.4 footer
                const size_t buffered = d->ibuf_len;
                const size_t drop = static_cast<size_t>(
                    (total < buffered) ? total : buffered);

                std::memmove(
                    d->ibuf, d->ibuf + drop, buffered - drop);
                d->ibuf_len = buffered - drop;
                d->id3_skip_remaining = total - drop;
                d->id3_state = (d->id3_skip_remaining > 0)
                                   ? Id3State::SKIPPING
                                   : Id3State::DONE;

                if (d->id3_state == Id3State::SKIPPING) {
                    if (sync_state) *sync_state = d->sync_state;
                    return -2;
                }
            }
        }
    }

    // ── Phase 1: Sync acquisition ────────────────────────────────────
    if (d->sync_state == SyncState::LOST) {
        // Once the scan budget is exhausted, discard input until an
        // explicit reset (flush/NewSegment) instead of rescanning forever.
        if (d->scan_exhausted) {
            d->ibuf_len = 0;
            d->stream.buffer    = nullptr;
            d->stream.bufend    = nullptr;
            d->stream.this_frame = nullptr;
            d->stream.next_frame = nullptr;
            if (sync_state) *sync_state = d->sync_state;
            return -2;
        }

        // Check if we've exceeded the scan limit
        d->bytes_scanned += append_len;
        if (d->bytes_scanned > MAX_SYNC_SCAN_BYTES) {
            // Give up — we've scanned too much without finding a frame.
            d->scan_exhausted = true;
            d->ibuf_len = 0;
            d->stream.buffer    = nullptr;
            d->stream.bufend    = nullptr;
            d->stream.this_frame = nullptr;
            d->stream.next_frame = nullptr;
            if (sync_state) *sync_state = d->sync_state;
            return -2;  // Not an error — just no output yet
        }

        // Try to find a valid frame header
        const size_t frame_offset = mpeg_find_frame(d->ibuf, d->ibuf_len);
        if (frame_offset == SIZE_MAX) {
            // Retain an incomplete candidate, or the last three bytes of a
            // possible split header. Never discard the entire sync window.
            size_t keep = d->ibuf_len > 3 ? d->ibuf_len - 3 : 0;
            for (size_t i = 0; i + 4 <= d->ibuf_len; ++i) {
                const size_t length = mpeg_frame_length(d->ibuf + i);
                if (length != 0 && length > d->ibuf_len - i) {
                    keep = i;
                    break;
                }
            }
            if (keep > 0) {
                std::memmove(d->ibuf, d->ibuf + keep, d->ibuf_len - keep);
                d->ibuf_len -= keep;
            }
            if (sync_state) *sync_state = d->sync_state;
            return -2;  // Need more data
        }

        // Release the scanned prefix even if confirmation needs more input.
        if (frame_offset > 0) {
            std::memmove(d->ibuf, d->ibuf + frame_offset, d->ibuf_len - frame_offset);
            d->ibuf_len -= frame_offset;
        }

        // A full frame at a chunk boundary is only tentative. Do not hand
        // it to libmad until the next header confirms sync; libmad can skip
        // the opening frame while probing an incomplete following header.
        // EOF is the exception, where the guard permits a single final frame.
        const size_t frame_length = mpeg_frame_length(d->ibuf);
        if (!end_of_stream &&
            d->ibuf_len - frame_length < 4) {
            if (sync_state) *sync_state = d->sync_state;
            return -2;
        }

        // Found a frame! Discard everything before it and feed to libmad.
        d->total_sync_attempts++;

        // A successful acquisition starts a fresh scan budget, even when
        // the frame happened to begin at offset zero.
        d->bytes_scanned = 0;
        d->scan_exhausted = false;

        // Enter SYNCING state. Metadata is discarded separately below.
        d->sync_state             = SyncState::SYNCING;
        d->preroll_remaining      = d->initial_preroll_frames;
        d->preroll_frames_decoded = 0;
    }

    // ── Phase 2 & 3: Decode loop (SYNCING → SYNCHRONIZED) ────────────
    // libmad requires the zeroed MAD_BUFFER_GUARD bytes to be part of the
    // final buffer length. A zero-length call is the explicit EOF drain.
    std::memset(d->ibuf + d->ibuf_len, 0, d->GUARD);
    feed_stream(d, end_of_stream);

    while (total_out < max_out_frames) {
        if (d->total_frames_decoded - start_frames >= max_frames) {
            compact_ibuf(d);
            break;
        }
        // Never decode a frame unless the output buffer can hold the
        // largest possible frame.  A frame that is decoded but does not
        // fit would be lost because libmad has already advanced past it.
        // Once the channel count is known, use the real bound so mono
        // streams can fill more of the output buffer.
        const size_t frame_bound =
            (d->channels == 1) ? (MAX_PCM_S16_PER_FRAME / 2)
                               : MAX_PCM_S16_PER_FRAME;
        if (max_out_frames - total_out < frame_bound) {
            compact_ibuf(d);
            break;
        }

        // Check if we've exceeded error limits
        if (d->consecutive_errors > MAX_CONSECUTIVE_ERRORS) {
            // Too many errors — reset sync
            maddec_soft_reset(d);
            compact_ibuf(d);
            break;
        }

        // Check if we've exceeded preroll limits
        if (d->sync_state == SyncState::SYNCING &&
            d->preroll_frames_decoded > MAX_PREROLL_FRAMES)
        {
            // We've discarded too many frames during preroll.
            // The stream may be corrupt. Reset and try again.
            maddec_soft_reset(d);
            compact_ibuf(d);
            break;
        }

        // ── Decode one frame ─────────────────────────────────────────
        if (mad_frame_decode(&d->frame, &d->stream) != 0) {
            // BUFLEN / BUFPTR are NOT MAD_RECOVERABLE (error codes
            // 0x0001, 0x0002 have high byte == 0).  Check them first.
            if (d->stream.error == MAD_ERROR_BUFLEN ||
                d->stream.error == MAD_ERROR_BUFPTR) {
                // Need more data — compact and wait for next chunk
                compact_ibuf(d);
                break;
            }

            // The zeroed MAD_BUFFER_GUARD tail is not a lost audio
            // frame.  libmad reports LOSTSYNC for the guard bytes during
            // an EOF drain; treat that as end-of-buffer, not concealment.
            if (d->stream.error == MAD_ERROR_LOSTSYNC &&
                d->stream.next_frame &&
                d->stream.next_frame >= d->ibuf + d->ibuf_len) {
                compact_ibuf(d);
                break;
            }

            d->total_decode_errors++;
            d->last_error = d->stream.error;

            const bool first_error = d->consecutive_errors == 0;
            d->consecutive_errors++;
            const bool was_synchronized =
                d->sync_state == SyncState::SYNCHRONIZED;

            // During preroll, a bad frame suggests the reservoir may
            // need more history.  Extend the discard window one frame
            // per error, bounded by MAX_PREROLL_FRAMES.
            if (!was_synchronized &&
                d->sync_state == SyncState::SYNCING &&
                d->preroll_frames_decoded <= MAX_PREROLL_FRAMES) {
                d->preroll_frames_decoded++;
                d->preroll_remaining++;
            }

            if (MAD_RECOVERABLE(d->stream.error)) {
                // The 0x02xx audio-data errors follow a parsed frame header:
                // each damaged frame needs its own replacement duration.
                // Header/sync errors can repeat while scanning bytes, so
                // conceal only once per burst for those errors.
                const bool damaged_frame =
                    (d->stream.error & 0xFF00) == 0x0200;
                if (was_synchronized && (first_error || damaged_frame)) {
                    const bool concealed = write_concealment(
                        d, out, max_out_frames, &total_out);
                    if (!concealed &&
                        d->concealment_frames >= MAX_CONCEAL_FRAMES) {
                        maddec_soft_reset(d);
                        compact_ibuf(d);
                        break;
                    }
                }
                continue;
            }

            // Non-recoverable — conceal what we can, then re-sync.
            if (was_synchronized && first_error) {
                write_concealment(d, out, max_out_frames, &total_out);
            }
            if (was_synchronized) {
                maddec_soft_reset(d);
            }
            compact_ibuf(d);
            break;
        }

        // ── Frame decoded successfully ───────────────────────────────
        // Reject transitions before mixing incompatible PCM into a sample.
        // A full reset explicitly permits a new stream format.
        if (d->sample_rate != 0 &&
            (d->sample_rate != d->frame.header.samplerate ||
             d->channels != MAD_NCHANNELS(&d->frame.header))) {
            d->format_error = true;
            if (sync_state) *sync_state = d->sync_state;
            return -1;
        }
        d->consecutive_errors = 0;
        d->total_frames_decoded++;   // count ALL frames, including preroll
        if (d->sync_state == SyncState::SYNCING &&
            d->preroll_frames_decoded == 0 &&
            d->stream.this_frame && d->stream.next_frame &&
            mpeg_frame_is_metadata(d->stream.this_frame,
                static_cast<size_t>(d->stream.next_frame - d->stream.this_frame))) {
            // Metadata has no audio duration and consumes no audio preroll.
            ++d->total_frames_discarded;
            continue;
        }
        mad_timer_add(&d->timer, d->frame.header.duration);
        mad_synth_frame(&d->synth, &d->frame);

        // Record format from first frame
        if (d->sample_rate == 0) {
            d->sample_rate = d->synth.pcm.samplerate;
            d->channels    = static_cast<uint8_t>(d->synth.pcm.channels);
        }

        // Keep the last-good frame shape for timeline-preserving concealment.
        d->last_frame_duration = d->frame.header.duration;
        d->last_frame_samples  = d->synth.pcm.length;
        d->last_frame_channels = static_cast<uint8_t>(d->synth.pcm.channels);
        d->concealment_frames  = 0;

        if (d->sync_state == SyncState::SYNCING) {
            // Preroll: decode but discard the PCM output
            d->preroll_frames_decoded++;

            if (d->preroll_remaining > 0) {
                d->preroll_remaining--;
            }

            if (d->preroll_remaining <= 0) {
                // Preroll complete — enter synchronized state
                d->sync_state = SyncState::SYNCHRONIZED;
                // Fall through to output this frame's PCM below
            } else {
                // Still in preroll — skip output, continue loop
                d->total_frames_discarded++;
                continue;
            }
        }

        // ── Output PCM ───────────────────────────────────────────────
        size_t frames_available = max_out_frames - total_out;
        size_t written = write_pcm(&d->synth, out + total_out, frames_available);

        if (written == 0) {
            // Unreachable after the capacity check above.  Never silently
            // discard a decoded frame; surface a fatal decoder error.
            return -1;
        }

        total_out += written;
    }

    // A loop that stops because the output buffer filled exits through the
    // while condition rather than a break path. Compact once in that case so
    // the frame just emitted is not decoded again on the next call.
    if (total_out > 0 && total_out == max_out_frames) {
        compact_ibuf(d);
    }

    *out_written = total_out * sizeof(int16_t);
    if (sync_state) *sync_state = d->sync_state;

    return (d->ibuf_len == 0) ? -2 : 0;
}

int maddec_decode(MadDecoder* d,
                  const uint8_t* in,  size_t  in_len,
                  int16_t*        out, size_t  out_capacity,
                  size_t*         in_consumed,
                  size_t*         out_written,
                  SyncState*      sync_state,
                  size_t          max_frames)
{
    return maddec_decode_internal(
        d, in, in_len, in_len == 0, out, out_capacity,
        in_consumed, out_written, sync_state, max_frames);
}

int maddec_decode_buffered(MadDecoder* d,
                           int16_t* out, size_t out_capacity,
                           size_t* in_consumed,
                           size_t* out_written,
                           SyncState* sync_state,
                           size_t max_frames)
{
    return maddec_decode_internal(
        d, nullptr, 0, false, out, out_capacity,
        in_consumed, out_written, sync_state, max_frames);
}

} // namespace madfilter
