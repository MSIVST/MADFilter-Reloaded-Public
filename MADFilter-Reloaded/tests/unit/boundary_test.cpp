// boundary_test.cpp — Boundary / edge-case tests for MADFilter Reloaded
//
// Tests BoundedBuffer, mpeg_frame_length, and the decoder wrapper
// at their exact boundary conditions.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <vector>

#include "common/bounded_buffer.h"
#include "filter/maddecode.h"

static int tests = 0, passed = 0, failed = 0;

#define TEST(name) do { tests++; fprintf(stderr, "  %-50s ", name); } while(0)
#define OK()  do { passed++; fprintf(stderr, "PASS\n"); } while(0)
#define FAIL(msg) do { failed++; fprintf(stderr, "FAIL: %s\n", msg); } while(0)

// ── BoundedBuffer tests ───────────────────────────────────────────────

static void test_bounded_buffer() {
    fprintf(stderr, "\n--- BoundedBuffer ---\n");

    // Empty buffer
    TEST("empty buffer has 0 available");
    madfilter::BoundedBuffer bb;
    if (bb.available() == 0 && bb.empty()) OK(); else FAIL("not empty");

    // Write within capacity
    TEST("write & read within capacity");
    uint8_t data[100];
    for (int i = 0; i < 100; i++) data[i] = (uint8_t)i;
    size_t w = bb.write(data, 100);
    if (w == 100 && bb.available() == 100) OK(); else FAIL("wrong count");

    // Read and consume
    TEST("consume reduces available");
    bb.consume(50);
    if (bb.available() == 50) OK(); else FAIL("wrong available after consume");

    // Compact
    TEST("compact moves data to front");
    bb.compact();
    if (bb.available() == 50) {
        const uint8_t* p = bb.read_ptr();
        if (p[0] == 50 && p[1] == 51) OK(); else FAIL("wrong data after compact");
    } else FAIL("wrong available after compact");

    // Write past capacity
    TEST("write past capacity is bounded");
    std::vector<uint8_t> big(2 * 1024 * 1024, 0xAA);
    w = bb.write(big.data(), big.size());
    if (w <= madfilter::MAD_INPUT_BUFFER_CAPACITY) OK(); else FAIL("exceeded capacity");

    // Reset
    TEST("reset clears buffer");
    bb.reset();
    if (bb.available() == 0 && bb.empty()) OK(); else FAIL("not empty after reset");

    // Null write
    TEST("write 0 bytes returns 0");
    w = bb.write(nullptr, 0);
    if (w == 0) OK(); else FAIL("wrote with null");

    TEST("null write with nonzero length returns 0");
    w = bb.write(nullptr, 1);
    if (w == 0) OK(); else FAIL("wrote from null source");
}

// ── mpeg_frame_length boundary tests ─────────────────────────────────

static void test_frame_header() {
    fprintf(stderr, "\n--- mpeg_frame_length ---\n");

    // All 256 possible byte-2 values with valid byte-0 and byte-1
    TEST("all second-byte variants handled");
    int valid = 0, invalid = 0;
    for (int b1 = 0; b1 < 256; b1++) {
        uint8_t hdr[4] = {0xFF, (uint8_t)b1, 0x90, 0x00};
        uint32_t len = madfilter::mpeg_frame_length(hdr);
        if (len > 0) valid++; else invalid++;
    }
    if (valid > 0 && invalid > 0) OK(); else FAIL("all or none accepted");

    // All 256^2 byte-2/byte-3 combos with valid byte-0/byte-1
    TEST("all bitrate/samplerate combos handled");
    valid = 0;
    for (int b2 = 0; b2 < 256; b2++) {
        for (int b3 = 0; b3 < 256; b3++) {
            uint8_t hdr[4] = {0xFF, 0xFB, (uint8_t)b2, (uint8_t)b3};
            uint32_t len = madfilter::mpeg_frame_length(hdr);
            if (len > 0) valid++;
        }
    }
    // Should have some valid combinations
    // 168 valid b2 values × 256 ignored b3 values = 43,008 valid combos
    if (valid == 43008) OK(); else {
        char buf[64]; snprintf(buf, sizeof(buf), "%d valid combos", valid); FAIL(buf);
    }

    // Byte 0 != 0xFF always returns 0
    TEST("non-0xFF first byte rejected");
    for (int b0 = 0; b0 < 256; b0++) {
        if (b0 == 0xFF) continue;
        uint8_t hdr[4] = {(uint8_t)b0, 0xFB, 0x90, 0x00};
        if (madfilter::mpeg_frame_length(hdr) != 0) { FAIL("accepted non-sync"); return; }
    }
    OK();

    // All zeros
    TEST("zero header rejected");
    uint8_t zero[4] = {0,0,0,0};
    if (madfilter::mpeg_frame_length(zero) == 0) OK(); else FAIL("accepted zeros");

    // All 0xFF
    TEST("all-0xFF header rejected");
    uint8_t ff[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t len = madfilter::mpeg_frame_length(ff);
    // 0xFFFF: version=3 (MPEG1), layer=3 (Layer I), bitrate=15 (forbidden)
    // Should be rejected due to forbidden bitrate
    if (len == 0) OK(); else FAIL("accepted forbidden bitrate");
}

// ── MPEG frame-length matrix ─────────────────────────────────────────

static void make_header(uint8_t header[4],
                        unsigned version_idx,
                        unsigned layer_idx,
                        unsigned bitrate_idx,
                        unsigned samplerate_idx,
                        unsigned padding)
{
    header[0] = 0xFF;
    header[1] = static_cast<uint8_t>(
        0xE0 | (version_idx << 3) | (layer_idx << 1) | 0x01);
    header[2] = static_cast<uint8_t>(
        (bitrate_idx << 4) | (samplerate_idx << 2) | (padding << 1));
    header[3] = 0;
}

static void test_frame_length_matrix() {
    fprintf(stderr, "\n--- MPEG frame lengths ---\n");

    struct Case {
        const char* name;
        unsigned version;
        unsigned layer;
        unsigned bitrate;
        unsigned rate;
        unsigned padding;
        uint32_t expected;
    };

    const Case cases[] = {
        // version: 3=MPEG1, 2=MPEG2, 0=MPEG2.5
        // layer:   3=Layer I, 2=Layer II, 1=Layer III
        {"MPEG1 L3 128k 44.1k",    3, 1,  9, 0, 0,  417},
        {"MPEG1 L3 128k 44.1k pad",3, 1,  9, 0, 1,  418},
        {"MPEG2 L3 56k 22.05k",    2, 1,  7, 0, 0,  182},
        {"MPEG2 L3 56k 22.05k pad",2, 1,  7, 0, 1,  183},
        {"MPEG2.5 L3 8k 8k",       0, 1,  1, 2, 0,   72},
        {"MPEG2.5 L3 8k 8k pad",   0, 1,  1, 2, 1,   73},
        {"MPEG1 L2 128k 44.1k",    3, 2,  8, 0, 0,  417},
        {"MPEG2 L2 56k 22.05k",    2, 2,  7, 0, 0,  365},
        {"MPEG1 L1 128k 44.1k",    3, 3,  4, 0, 0,  136},
        {"MPEG1 L1 128k 44.1k pad",3, 3,  4, 0, 1,  140},
        {"MPEG2 L1 32k 22.05k",    2, 3,  1, 0, 0,   68},
        {"MPEG2.5 L1 32k 8k",      0, 3,  1, 2, 0,  192},
    };

    for (const Case& test : cases) {
        uint8_t header[4];
        make_header(header, test.version, test.layer, test.bitrate,
                    test.rate, test.padding);
        const uint32_t actual = madfilter::mpeg_frame_length(header);
        char name[96];
        snprintf(name, sizeof(name), "%s = %u", test.name,
                 static_cast<unsigned>(test.expected));
        TEST(name);
        if (actual == test.expected) {
            OK();
        } else {
            char message[64];
            snprintf(message, sizeof(message), "got %u",
                     static_cast<unsigned>(actual));
            FAIL(message);
        }
    }

    TEST("MPEG-2 Layer III is not MPEG-1 length");
    uint8_t mpeg2[4];
    make_header(mpeg2, 2, 1, 7, 0, 0);
    if (madfilter::mpeg_frame_length(mpeg2) == 182) OK();
    else FAIL("MPEG-2 L3 used the MPEG-1 formula");
}

// ── Frame scanner validation ──────────────────────────────────────────

static void make_frame(std::vector<uint8_t>* data, size_t count,
                       unsigned version, unsigned layer,
                       unsigned bitrate, unsigned rate,
                       unsigned channels = 2)
{
    uint8_t header[4];
    make_header(header, version, layer, bitrate, rate, 0);
    header[3] = channels == 1 ? 0xC0 : 0x00;
    const uint32_t frame_len = madfilter::mpeg_frame_length(header);
    data->assign(static_cast<size_t>(frame_len) * count, 0);
    for (size_t i = 0; i < count; ++i) {
        std::memcpy(data->data() + i * frame_len, header, 4);
    }
}

static void test_frame_scanner() {
    fprintf(stderr, "\n--- MPEG frame scanner ---\n");

    TEST("adjacent MPEG-1 frames accepted");
    std::vector<uint8_t> frames;
    make_frame(&frames, 2, 3, 1, 9, 0);
    if (madfilter::mpeg_find_frame(frames.data(), frames.size()) == 0) OK();
    else FAIL("did not find first frame");

    TEST("adjacent MPEG-2 Layer III frames accepted");
    make_frame(&frames, 2, 2, 1, 7, 0);
    if (madfilter::mpeg_find_frame(frames.data(), frames.size()) == 0) OK();
    else FAIL("did not find MPEG-2 frame");

    TEST("incomplete first frame rejected");
    make_frame(&frames, 1, 3, 1, 9, 0);
    frames.resize(frames.size() / 2);
    if (madfilter::mpeg_find_frame(frames.data(), frames.size()) == SIZE_MAX) OK();
    else FAIL("accepted a frame that extends past the buffer");

    TEST("mismatched second header rejected");
    std::vector<uint8_t> first;
    make_frame(&first, 1, 3, 1, 9, 0); // MPEG-1 L3
    uint8_t second[4];
    make_header(second, 2, 1, 7, 0, 0); // MPEG-2 L3
    const size_t second_pos = first.size();
    first.resize(second_pos + 4, 0);
    std::memcpy(first.data() + second_pos, second, 4);
    if (madfilter::mpeg_find_frame(first.data(), first.size()) != 0) OK();
    else FAIL("accepted incompatible second header");

    TEST("short tail does not over-read");
    make_frame(&frames, 1, 3, 1, 9, 0);
    frames.resize(frames.size() + 2, 0);
    if (madfilter::mpeg_find_frame(frames.data(), frames.size()) == 0) OK();
    else FAIL("did not accept a complete frame with a short tail");
}

// ── ID3v2 skipping ────────────────────────────────────────────────────

static std::vector<uint8_t> make_id3v2_tag(size_t payload_size) {
    std::vector<uint8_t> tag(10 + payload_size, 0);
    tag[0] = 'I';
    tag[1] = 'D';
    tag[2] = '3';
    tag[3] = 4;  // ID3v2.4
    tag[4] = 0;
    tag[5] = 0;  // no footer

    // Syncsafe payload size.
    const uint32_t size = static_cast<uint32_t>(payload_size);
    tag[6] = static_cast<uint8_t>((size >> 21) & 0x7F);
    tag[7] = static_cast<uint8_t>((size >> 14) & 0x7F);
    tag[8] = static_cast<uint8_t>((size >> 7) & 0x7F);
    tag[9] = static_cast<uint8_t>(size & 0x7F);

    // Fill the payload with plausible false frame headers.
    for (size_t i = 10; i + 4 <= tag.size(); i += 417) {
        tag[i] = 0xFF;
        tag[i + 1] = 0xFB;
        tag[i + 2] = 0x90;
        tag[i + 3] = 0x00;
    }
    return tag;
}

static void test_id3_skip() {
    fprintf(stderr, "\n--- ID3v2 skipping ---\n");

    // Layer II frames have valid silent payloads, unlike synthetic
    // Layer III frames whose side information is all zero.
    std::vector<uint8_t> audio;
    make_frame(&audio, 8, 3, 2, 8, 0);
    std::vector<uint8_t> tag = make_id3v2_tag(417 * 50);
    tag.insert(tag.end(), audio.begin(), audio.end());

    std::unique_ptr<madfilter::MadDecoder> dec(
        new (std::nothrow) madfilter::MadDecoder);
    if (!dec) {
        TEST("ID3v2 tag is skipped");
        FAIL("decoder allocation failed");
        return;
    }
    madfilter::maddec_init(dec.get());

    std::vector<int16_t> pcm(8 * 1152 * 2);
    size_t offset = 0;
    size_t total_int16 = 0;
    bool ok = true;

    for (size_t calls = 0; offset < tag.size() && calls < 1024; ++calls) {
        size_t consumed = 0;
        size_t written = 0;
        madfilter::SyncState state;
        const int result = madfilter::maddec_decode(
            dec.get(), tag.data() + offset, tag.size() - offset,
            pcm.data(), pcm.size() * sizeof(int16_t),
            &consumed, &written, &state);
        if (result == -1 || (consumed == 0 && written == 0)) {
            ok = false;
            break;
        }
        offset += consumed;
        total_int16 += written / sizeof(int16_t);
    }

    const uint8_t empty = 0;
    if (ok) {
        for (size_t i = 0; i < 8; ++i) {
            size_t consumed = 0;
            size_t written = 0;
            madfilter::SyncState state;
            const int result = madfilter::maddec_decode(
                dec.get(), &empty, 0, pcm.data(),
                pcm.size() * sizeof(int16_t),
                &consumed, &written, &state);
            if (result == -1 || consumed != 0) {
                ok = false;
                break;
            }
            total_int16 += written / sizeof(int16_t);
            if (written == 0) break;
        }
    }

    TEST("ID3v2 tag is skipped");
    // 8 frames minus 5 preroll frames = 4 emitted frames.
    const size_t expected_int16 = 4 * 1152 * 2;
    if (ok && total_int16 == expected_int16 &&
        dec->sync_state == madfilter::SyncState::SYNCHRONIZED &&
        dec->sample_rate == 44100 && dec->channels == 2) {
        OK();
    } else {
        char message[192];
        snprintf(message, sizeof(message),
                 "got %zu int16, %u Hz %u ch, frames=%llu disc=%llu "
                 "sync=%llu concealed=%llu offset=%zu/%zu, expected %zu",
                 total_int16, dec->sample_rate, dec->channels,
                 (unsigned long long)dec->total_frames_decoded,
                 (unsigned long long)dec->total_frames_discarded,
                 (unsigned long long)dec->total_sync_attempts,
                 (unsigned long long)dec->total_concealed_frames,
                 offset, tag.size(), expected_int16);
        FAIL(message);
    }

    madfilter::maddec_finish(dec.get());
}

// ── Concurrent per-instance decoder state ─────────────────────────────

static void test_decoder_concurrency() {
    fprintf(stderr, "\n--- Concurrent decoder instances ---\n");

    constexpr size_t kFrames = 30;
    constexpr size_t kChunk = 13;
    constexpr size_t kStereoInt16 = 1152 * 2;
    constexpr size_t kAltInt16 = 1152 * 2;
    constexpr size_t kOutputFrames = kFrames - 5 + 1;

    std::vector<uint8_t> stereo;
    std::vector<uint8_t> alternate;
    make_frame(&stereo, kFrames, 3, 2, 8, 0, 2);
    make_frame(&alternate, kFrames, 3, 2, 8, 1, 2);

    std::unique_ptr<madfilter::MadDecoder> a(
        new (std::nothrow) madfilter::MadDecoder);
    std::unique_ptr<madfilter::MadDecoder> b(
        new (std::nothrow) madfilter::MadDecoder);
    if (!a || !b) {
        TEST("interleaved 44.1 and 48 kHz decoders stay independent");
        FAIL("decoder allocation failed");
        return;
    }
    madfilter::maddec_init(a.get());
    madfilter::maddec_init(b.get());

    std::vector<int16_t> pcm_a(kStereoInt16);
    std::vector<int16_t> pcm_b(kAltInt16);
    size_t offset_a = 0;
    size_t offset_b = 0;
    size_t total_a = 0;
    size_t total_b = 0;
    bool ok = true;

    for (size_t calls = 0; calls < 4096; ++calls) {
        bool progressed = false;

        if (offset_a < stereo.size()) {
            const size_t chunk =
                (std::min)(kChunk, stereo.size() - offset_a);
            size_t consumed = 0;
            size_t written = 0;
            madfilter::SyncState state;
            const int result = madfilter::maddec_decode(
                a.get(), stereo.data() + offset_a, chunk,
                pcm_a.data(), pcm_a.size() * sizeof(int16_t),
                &consumed, &written, &state);
            if (result == -1) { ok = false; break; }
            offset_a += consumed;
            total_a += written / sizeof(int16_t);
            progressed = progressed || consumed > 0 || written > 0;
        }

        if (offset_b < alternate.size()) {
            const size_t chunk =
                (std::min)(kChunk, alternate.size() - offset_b);
            size_t consumed = 0;
            size_t written = 0;
            madfilter::SyncState state;
            const int result = madfilter::maddec_decode(
                b.get(), alternate.data() + offset_b, chunk,
                pcm_b.data(), pcm_b.size() * sizeof(int16_t),
                &consumed, &written, &state);
            if (result == -1) { ok = false; break; }
            offset_b += consumed;
            total_b += written / sizeof(int16_t);
            progressed = progressed || consumed > 0 || written > 0;
        }

        if (offset_a == stereo.size() && offset_b == alternate.size()) {
            break;
        }
        if (!progressed) { ok = false; break; }
    }

    const uint8_t empty = 0;
    for (madfilter::MadDecoder* decoder : {a.get(), b.get()}) {
        std::vector<int16_t>& pcm = (decoder == a.get()) ? pcm_a : pcm_b;
        size_t* total = (decoder == a.get()) ? &total_a : &total_b;
        for (size_t i = 0; i < 16; ++i) {
            size_t consumed = 0;
            size_t written = 0;
            madfilter::SyncState state;
            const int result = madfilter::maddec_decode(
                decoder, &empty, 0, pcm.data(),
                pcm.size() * sizeof(int16_t),
                &consumed, &written, &state);
            if (result == -1 || consumed != 0) { ok = false; break; }
            *total += written / sizeof(int16_t);
            if (written == 0) break;
        }
    }

    TEST("interleaved 44.1 and 48 kHz decoders stay independent");
    if (ok &&
        a->sample_rate == 44100 && a->channels == 2 &&
        b->sample_rate == 48000 && b->channels == 2 &&
        total_a == kOutputFrames * kStereoInt16 &&
        total_b == kOutputFrames * kAltInt16) {
        OK();
    } else {
        char message[160];
        snprintf(message, sizeof(message),
                 "A=%zu/%zu %uHz%uch B=%zu/%zu %uHz%uch",
                 total_a, kOutputFrames * kStereoInt16,
                 a->sample_rate, a->channels,
                 total_b, kOutputFrames * kAltInt16,
                 b->sample_rate, b->channels);
        FAIL(message);
    }

    madfilter::maddec_finish(a.get());
    madfilter::maddec_finish(b.get());
}

// ── Timeline-preserving concealment ──────────────────────────────────

static void test_decoder_concealment(size_t bad_frames = 1,
                                     bool crc_errors = false) {
    fprintf(stderr, "\n--- Decoder concealment ---\n");

    constexpr size_t kFrameBytes = 417;
    constexpr size_t kFrameCount = 20;
    constexpr size_t kBadFrame = 9;  // zero-based frame index
    constexpr size_t kPcmInt16PerFrame = 1152 * 2;

    std::vector<uint8_t> frames(kFrameBytes * kFrameCount, 0);
    for (size_t i = 0; i < kFrameCount; ++i) {
        uint8_t* frame = frames.data() + i * kFrameBytes;
        frame[0] = 0xFF; // MPEG-1 Layer II, 128 kbps, 44.1 kHz stereo
        frame[1] = 0xFD;
        frame[2] = 0x80;
        frame[3] = 0x00;
    }
    if (crc_errors) {
        for (size_t i = 0; i < bad_frames; ++i) {
            // A parsed frame with a wrong CRC still occupies one frame of
            // playback time, including when several damaged frames are adjacent.
            frames[(kBadFrame + i) * kFrameBytes + 1] = 0xFC;
        }
    } else {
        std::memset(
            frames.data() + kBadFrame * kFrameBytes, 0, kFrameBytes);
    }
    const char* test_name = crc_errors ?
        "adjacent CRC errors preserve playback duration" :
        "bad frame conceals one frame of silence";

    std::unique_ptr<madfilter::MadDecoder> dec(
        new (std::nothrow) madfilter::MadDecoder);
    if (!dec) {
        TEST(test_name);
        FAIL("decoder allocation failed");
        return;
    }
    maddec_init(dec.get());

    std::vector<int16_t> pcm(
        (kFrameCount + 4) * kPcmInt16PerFrame);
    size_t offset = 0;
    size_t total_int16 = 0;
    int result = 0;
    madfilter::SyncState state;

    for (size_t calls = 0; offset < frames.size() && calls < 64; ++calls) {
        size_t consumed = 0;
        size_t written = 0;
        result = madfilter::maddec_decode(
            dec.get(), frames.data() + offset, frames.size() - offset,
            pcm.data(), pcm.size() * sizeof(int16_t),
            &consumed, &written, &state);
        if (result == -1 || (consumed == 0 && written == 0)) break;
        offset += consumed;
        total_int16 += written / sizeof(int16_t);
    }

    const uint8_t empty = 0;
    if (result != -1 && offset == frames.size()) {
        for (size_t i = 0; i < 8; ++i) {
            size_t consumed = 0;
            size_t written = 0;
            result = madfilter::maddec_decode(
                dec.get(), &empty, 0,
                pcm.data(), pcm.size() * sizeof(int16_t),
                &consumed, &written, &state);
            if (result == -1 || consumed != 0) break;
            total_int16 += written / sizeof(int16_t);
            if (written == 0) break;
        }
    }

    TEST(test_name);
    const size_t expected_int16 =
        (kFrameCount - 5 + 1) * kPcmInt16PerFrame;
    if (result != -1 &&
        total_int16 == expected_int16 &&
        dec->total_concealed_frames == bad_frames &&
        (!crc_errors ||
         (dec->last_error == MAD_ERROR_BADCRC &&
          dec->total_decode_errors == bad_frames)) &&
        dec->sync_state == madfilter::SyncState::SYNCHRONIZED) {
        OK();
    } else {
        char message[128];
        snprintf(message, sizeof(message),
                 "got %zu int16, concealed=%llu, expected %zu",
                 total_int16,
                 (unsigned long long)dec->total_concealed_frames,
                 expected_int16);
        FAIL(message);
    }

    madfilter::maddec_finish(dec.get());
}

// ── Decoder no-drop test ──────────────────────────────────────────────

static void test_decoder_no_drop() {
    fprintf(stderr, "\n--- Decoder no-drop ---\n");

    constexpr size_t kFrameBytes = 417;
    constexpr size_t kFrameCount = 30;
    constexpr size_t kPrerollFrames = 5;
    constexpr size_t kPcmInt16PerFrame = 1152 * 2;

    std::vector<uint8_t> frames(kFrameBytes * kFrameCount, 0);
    for (size_t i = 0; i < kFrameCount; ++i) {
        uint8_t* frame = frames.data() + i * kFrameBytes;
        frame[0] = 0xFF; // MPEG-1 Layer II, 128 kbps, 44.1 kHz stereo
        frame[1] = 0xFD;
        frame[2] = 0x80;
        frame[3] = 0x00;
    }

    std::unique_ptr<madfilter::MadDecoder> dec(
        new (std::nothrow) madfilter::MadDecoder);
    if (!dec) {
        TEST("no-drop decode");
        FAIL("decoder allocation failed");
        return;
    }
    madfilter::maddec_init(dec.get());

    std::vector<int16_t> pcm(kPcmInt16PerFrame);
    size_t offset = 0;
    size_t total_int16 = 0;
    bool input_done = false;
    bool ok = true;
    const uint8_t empty = 0;

    for (size_t calls = 0; calls < 256; ++calls) {
        const uint8_t* input = input_done ? &empty : frames.data() + offset;
        const size_t input_len = input_done ? 0 : frames.size() - offset;
        size_t consumed = 0;
        size_t written = 0;
        madfilter::SyncState state;
        const int result = madfilter::maddec_decode(
            dec.get(), input, input_len,
            pcm.data(), pcm.size() * sizeof(int16_t),
            &consumed, &written, &state);

        if (result == -1 || consumed > input_len) {
            ok = false;
            break;
        }
        offset += consumed;
        total_int16 += written / sizeof(int16_t);

        if (!input_done && offset == frames.size()) {
            input_done = true;
        }
        if (input_done && written == 0) {
            break;
        }
        if (!input_done && written == 0 && consumed == 0) {
            ok = false;
            break;
        }
    }

    TEST("one-frame output buffer loses no PCM");
    const size_t expected_int16 =
        (kFrameCount - kPrerollFrames + 1) * kPcmInt16PerFrame;
    if (ok && total_int16 == expected_int16) {
        OK();
    } else {
        char message[96];
        snprintf(message, sizeof(message),
                 "got %zu int16, expected %zu",
                 total_int16, expected_int16);
        FAIL(message);
    }

    madfilter::maddec_finish(dec.get());
}

// ── Decoder boundary tests ────────────────────────────────────────────

static void test_decoder_boundary() {
    fprintf(stderr, "\n--- Decoder boundary ---\n");

    // Null pointers
    TEST("null decoder rejected");
    madfilter::SyncState ss;
    size_t c, w;
    const uint8_t one_input = 0;
    int16_t one_output = 0;
    int r = madfilter::maddec_decode(
        nullptr, &one_input, 1, &one_output, sizeof(one_output),
        &c, &w, &ss);
    if (r == -1 && c == 0 && w == 0) OK(); else FAIL("didn't reject null");

    // Valid decoder, null input
    TEST("null input rejected");
    std::unique_ptr<madfilter::MadDecoder> dec(
        new (std::nothrow) madfilter::MadDecoder);
    if (!dec) {
        FAIL("decoder allocation failed");
        return;
    }
    madfilter::maddec_init(dec.get());
    r = madfilter::maddec_decode(
        dec.get(), nullptr, 100, &one_output, sizeof(one_output),
        &c, &w, &ss);
    if (r == -1) OK(); else FAIL("accepted null input");
    madfilter::maddec_finish(dec.get());

    // Zero-length input
    TEST("zero-length input handled");
    madfilter::maddec_init(dec.get());
    r = madfilter::maddec_decode(
        dec.get(), &one_input, 0, &one_output, sizeof(one_output),
        &c, &w, &ss);
    if (r == -2 && c == 0 && w == 0) OK(); else FAIL("didn't handle empty");
    madfilter::maddec_finish(dec.get());

    // Exactly MAD_BUFFER_GUARD-sized input
    TEST("guard-sized input");
    madfilter::maddec_init(dec.get());
    uint8_t guard_input[8] = {0,0,0,0,0,0,0,0};
    int16_t out[256];
    r = madfilter::maddec_decode(dec.get(), guard_input, 8, out, sizeof(out), &c, &w, &ss);
    if (r == 0 || r == -2) OK(); else FAIL("crashed on guard-sized");
    madfilter::maddec_finish(dec.get());

    // An incomplete fragment is ordinary backpressure, not a decoder error.
    TEST("incomplete synchronized input is not an error");
    madfilter::maddec_init(dec.get());
    dec->sync_state = madfilter::SyncState::SYNCHRONIZED;
    uint8_t fragment = 0;
    r = madfilter::maddec_decode(
        dec.get(), &fragment, 1, out, sizeof(out), &c, &w, &ss);
    if ((r == 0 || r == -2) && c == 1 && w == 0 &&
        dec->consecutive_errors == 0 &&
        ss == madfilter::SyncState::SYNCHRONIZED) {
        OK();
    } else {
        FAIL("incomplete input changed error or sync state");
    }
    madfilter::maddec_finish(dec.get());

    // A zero-length call at EOF must expose the zeroed guard bytes to libmad
    // so the final complete frame can be decoded without a following header.
    TEST("zero-length input drains guarded tail");
    madfilter::maddec_init(dec.get());
    constexpr size_t frame_bytes = 417;
    constexpr size_t frame_count = 7;
    std::vector<uint8_t> frames(frame_bytes * frame_count, 0);
    for (size_t i = 0; i < frame_count; ++i) {
        uint8_t* frame = frames.data() + i * frame_bytes;
        frame[0] = 0xFF;
        frame[1] = 0xFD; // MPEG-1 Layer II
        frame[2] = 0x80; // 128 kbps, 44.1 kHz
        frame[3] = 0x00; // stereo, no padding
    }
    std::vector<int16_t> pcm(frame_count * 1152 * 2);
    r = madfilter::maddec_decode(
        dec.get(), frames.data(), frames.size(), pcm.data(),
        pcm.size() * sizeof(int16_t), &c, &w, &ss);
    const size_t first_written = w;
    const uint8_t empty = 0;
    r = madfilter::maddec_decode(
        dec.get(), &empty, 0, pcm.data(), pcm.size() * sizeof(int16_t),
        &c, &w, &ss);
    if ((r == 0 || r == -2) && c == 0 &&
        first_written == 2 * 1152 * 2 * sizeof(int16_t) &&
        w == 1152 * 2 * sizeof(int16_t)) {
        OK();
    } else {
        FAIL("final complete frame was not drained");
    }
    madfilter::maddec_finish(dec.get());

    // Rapid init/finish cycle
    TEST("rapid init/finish (100 cycles)");
    bool ok = true;
    for (int i = 0; i < 100; i++) {
        madfilter::maddec_init(dec.get());
        madfilter::maddec_finish(dec.get());
    }
    if (ok) OK(); else FAIL("leaked memory");
}

static void test_chunk_sync_confirmation() {
    TEST("chunk-edge frame waits for a confirming header");
    std::vector<uint8_t> frames;
    make_frame(&frames, 2, 3, 2, 8, 0);
    const size_t frame_bytes = madfilter::mpeg_frame_length(frames.data());
    madfilter::MadDecoder decoder;
    madfilter::maddec_init(&decoder);
    decoder.initial_preroll_frames = 0;
    int16_t pcm[madfilter::MAX_PCM_S16_PER_FRAME] = {};
    size_t consumed = 0, written = 0;
    madfilter::SyncState state;
    const int first = madfilter::maddec_decode(
        &decoder, frames.data(), frame_bytes, pcm, sizeof(pcm),
        &consumed, &written, &state);
    const bool waiting = first != -1 && consumed == frame_bytes &&
        written == 0 && state == madfilter::SyncState::LOST &&
        decoder.total_frames_decoded == 0;
    const int second = madfilter::maddec_decode(
        &decoder, frames.data() + frame_bytes, 16, pcm, sizeof(pcm),
        &consumed, &written, &state);
    const bool preserved = second != -1 && consumed == 16 &&
        written == sizeof(pcm) && decoder.total_frames_decoded == 1;
    madfilter::maddec_finish(&decoder);
    if (waiting && preserved) OK(); else FAIL("tentative sync lost the opening frame");
}

// ── Main ──────────────────────────────────────────────────────────────

static void test_review_regressions() {
    // A sync window must preserve partial frames irrespective of chunk size.
    for (size_t prefix : {size_t(8000), size_t(8190)}) {
        for (size_t chunk : {size_t(1), size_t(417), size_t(4096), size_t(8192)}) {
            std::vector<uint8_t> data;
            make_frame(&data, 10, 3, 1, 9, 0);
            data.insert(data.begin(), prefix, 0x55);
            auto d = std::make_unique<madfilter::MadDecoder>();
            madfilter::maddec_init(d.get());
            d->initial_preroll_frames = 0;
            std::vector<int16_t> pcm(32 * 2304);
            size_t offset = 0, total = 0;
            bool ok = true;
            for (size_t calls = 0; calls < 30000; ++calls) {
                size_t length = data.size() - offset;
                if (length > chunk) length = chunk;
                size_t consumed = 0, written = 0;
                int rc = madfilter::maddec_decode(d.get(), data.data() + offset,
                    length, pcm.data(), pcm.size() * 2, &consumed, &written, nullptr);
                total += written;
                offset += consumed;
                if (rc == -1 || (!consumed && !written && length)) { ok = false; break; }
                if (!length && !written) break;
            }
            TEST("sync window retains partial MP3 frame");
            if (ok && offset == data.size() && total == 10 * 4608 &&
                d->total_decode_errors == 0) OK(); else FAIL("chunking lost PCM");
            madfilter::maddec_finish(d.get());
        }
    }

    for (bool metadata : {false, true}) {
        std::vector<uint8_t> data;
        make_frame(&data, 10, 3, 1, 9, 0);
        if (metadata) std::memcpy(data.data() + 36, "Xing", 4);
        auto d = std::make_unique<madfilter::MadDecoder>();
        madfilter::maddec_init(d.get());
        d->initial_preroll_frames = 0;
        std::vector<int16_t> pcm(32 * 2304);
        size_t c = 0, w = 0, tail = 0;
        const int rc = madfilter::maddec_decode(d.get(), data.data(), data.size(),
            pcm.data(), pcm.size() * 2, &c, &w, nullptr);
        const uint8_t empty = 0;
        const int end_rc = madfilter::maddec_decode(d.get(), &empty, 0,
            pcm.data(), pcm.size() * 2, &c, &tail, nullptr);
        TEST("metadata is withheld without audio preroll");
        if (rc != -1 && end_rc != -1 && w + tail == (metadata ? 9u : 10u) * 4608 &&
            d->total_frames_discarded == (metadata ? 1u : 0u)) OK();
        else FAIL("metadata PCM or discard count incorrect");
        madfilter::maddec_finish(d.get());
    }

    for (bool rate_change : {false, true}) {
        std::vector<uint8_t> first, second;
        make_frame(&first, 10, 3, 1, 9, 0, 1);
        make_frame(&second, 10, 3, 1, 9, rate_change ? 1 : 0, rate_change ? 1 : 2);
        auto d = std::make_unique<madfilter::MadDecoder>();
        madfilter::maddec_init(d.get());
        d->initial_preroll_frames = 0;
        std::vector<int16_t> pcm(32 * 2304);
        size_t c = 0, w = 0;
        int rc = madfilter::maddec_decode(d.get(), first.data(), first.size(),
            pcm.data(), pcm.size() * 2, &c, &w, nullptr);
        const bool started = rc != -1 && w > 0;
        rc = madfilter::maddec_decode(d.get(), second.data(), second.size(),
            pcm.data(), pcm.size() * 2, &c, &w, nullptr);
        TEST("midstream format change rejects mixed PCM");
        if (started && rc == -1 && w == 0) OK(); else FAIL("mixed PCM accepted");
        madfilter::maddec_reset(d.get());
        rc = madfilter::maddec_decode(d.get(), second.data(), second.size(),
            pcm.data(), pcm.size() * 2, &c, &w, nullptr);
        TEST("explicit reset permits a new stream format");
        if (rc != -1 && w > 0 && d->channels == (rate_change ? 1 : 2) &&
            d->sample_rate == (rate_change ? 48000u : 44100u)) OK();
        else FAIL("reset retained format rejection");
        madfilter::maddec_finish(d.get());
    }

    TEST("buffer refill compacts consumed space safely");
    madfilter::BoundedBuffer bb;
    std::vector<uint8_t> data(madfilter::MAD_INPUT_BUFFER_CAPACITY, 0x5a);
    bb.write(data.data(), data.size());
    bb.consume(data.size() / 2);
    const uint8_t replacement[16] = {};
    const size_t written = bb.write(replacement, sizeof(replacement));
    if (written == sizeof(replacement) && bb.available() == data.size() / 2 + written &&
        bb.read_ptr()[0] == 0x5a && bb.read_ptr()[data.size() / 2] == 0) OK();
    else FAIL("refill did not preserve unread bytes");
}

int main() {
    test_review_regressions();
    test_bounded_buffer();
    test_frame_header();
    test_frame_length_matrix();
    test_frame_scanner();
    test_id3_skip();
    test_decoder_concurrency();
    test_decoder_concealment();
    test_decoder_concealment(2, true);
    test_decoder_concealment(3, true);
    test_decoder_no_drop();
    test_decoder_boundary();
    test_chunk_sync_confirmation();

    fprintf(stderr, "\n  %d tests: %d passed, %d failed\n", tests, passed, failed);
    return failed > 0 ? 1 : 0;
}
