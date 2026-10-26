// maddec_test.cpp — Command-line MP3 → WAV decoder test harness (Phase 2)
//
// Enhanced with arbitrary-offset testing, sync-state tracking,
// and preroll diagnostics.  Simulates Fallout 3 radio behavior:
// seeking, fragmented input, and station changes.

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <limits>
#include <memory>
#include <new>

#include "filter/maddecode.h"

constexpr size_t MAX_TEST_INPUT_SIZE = 8 * 1024 * 1024;

// ── WAV header (44 bytes, PCM) ────────────────────────────────────────
#pragma pack(push, 1)
struct WavHeader {
    char     riff_id[4];
    uint32_t riff_size;
    char     wave_id[4];
    char     fmt_id[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data_id[4];
    uint32_t data_size;
};
#pragma pack(pop)

static void wav_header_init(WavHeader* hdr, uint32_t sr, uint16_t ch, uint32_t ds) {
    std::memcpy(hdr->riff_id, "RIFF", 4);
    hdr->riff_size      = 36 + ds;
    std::memcpy(hdr->wave_id, "WAVE", 4);
    std::memcpy(hdr->fmt_id,  "fmt ", 4);
    hdr->fmt_size       = 16;
    hdr->audio_format   = 1;
    hdr->num_channels   = ch;
    hdr->sample_rate    = sr;
    hdr->bits_per_sample = 16;
    hdr->block_align    = ch * 2;
    hdr->byte_rate      = sr * hdr->block_align;
    std::memcpy(hdr->data_id, "data", 4);
    hdr->data_size      = ds;
}

// ── Decode entire file ────────────────────────────────────────────────
// Returns number of int16_t samples written to `out`.
static size_t decode_file(const uint8_t* mp3_data, size_t mp3_size,
                           int16_t* out, size_t out_capacity,
                           madfilter::MadDecoder* decoder,
                           size_t chunk_size,
                           bool verbose)
{
    size_t offset    = 0;
    size_t total_pcm = 0;
    int    stuck_count = 0;

    while (offset < mp3_size) {
        if (total_pcm >= out_capacity) {
            if (verbose) fprintf(stderr, "  Output buffer exhausted\n");
            return SIZE_MAX;
        }

        size_t chunk = mp3_size - offset;
        if (chunk_size > 0 && chunk > chunk_size) chunk = chunk_size;

        size_t consumed = 0;
        size_t written  = 0;
        madfilter::SyncState ss;

        int result = madfilter::maddec_decode(
            decoder,
            mp3_data + offset, chunk,
            out + total_pcm,
            (out_capacity - total_pcm) * sizeof(int16_t),
            &consumed, &written, &ss);

        const size_t output_bytes_remaining =
            (out_capacity - total_pcm) * sizeof(int16_t);
        if (consumed > chunk ||
            written > output_bytes_remaining ||
            written % sizeof(int16_t) != 0) {
            if (verbose) fprintf(stderr, "  Decoder returned invalid byte counts\n");
            return SIZE_MAX;
        }

        offset += consumed;
        size_t samples = written / sizeof(int16_t);
        total_pcm += samples;

        if (result == -1) {
            if (verbose) fprintf(stderr, "  Fatal decoder error at offset %zu\n", offset);
            return SIZE_MAX;
        }

        if (consumed == 0 && written == 0) {
            stuck_count++;
            if (stuck_count > 10) {
                if (verbose) fprintf(stderr, "  Stuck — giving up at offset %zu\n", offset);
                break;
            }
            // Advance one byte to try to re-sync
            offset++;
        } else {
            stuck_count = 0;
        }
    }
    return total_pcm;
}

static bool parse_size_argument(const char* text, size_t* value) {
    if (!text || !value || *text == '\0' || *text == '-') return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || !end || *end != '\0' ||
        parsed > (std::numeric_limits<size_t>::max)()) {
        return false;
    }
    *value = static_cast<size_t>(parsed);
    return true;
}

// ── Main ──────────────────────────────────────────────────────────────

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage:\n"
        "  %s <input.mp3> <output.wav>          Normal decode\n"
        "  %s --offset <offset> <input.mp3> <out.wav>  Decode from byte offset\n"
        "  %s --fragment <size> <input.mp3> <out.wav>  Fragment input into N-byte chunks\n"
        "  %s --test <input.mp3>                  Run offset test suite\n",
        prog, prog, prog, prog);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    const char* mode    = argv[1];
    const char* in_path = nullptr;
    const char* out_path = nullptr;
    size_t offset_param  = 0;
    size_t frag_size     = 0;
    bool run_suite       = false;

    if (strcmp(mode, "--offset") == 0) {
        if (argc != 5) { usage(argv[0]); return 1; }
        if (!parse_size_argument(argv[2], &offset_param)) {
            fprintf(stderr, "Invalid offset: %s\n", argv[2]);
            return 1;
        }
        in_path  = argv[3];
        out_path = argv[4];
    } else if (strcmp(mode, "--fragment") == 0) {
        if (argc != 5) { usage(argv[0]); return 1; }
        if (!parse_size_argument(argv[2], &frag_size) || frag_size == 0) {
            fprintf(stderr, "Invalid fragment size: %s\n", argv[2]);
            return 1;
        }
        in_path  = argv[3];
        out_path = argv[4];
    } else if (strcmp(mode, "--test") == 0) {
        in_path   = argv[2];
        run_suite = true;
    } else {
        // Default: normal decode
        in_path  = argv[1];
        out_path = argv[2];
    }

    // ── Read input file ───────────────────────────────────────────────
    FILE* fin = fopen(in_path, "rb");
    if (!fin) {
        fprintf(stderr, "Error: cannot open '%s'\n", in_path);
        return 2;
    }
    fseek(fin, 0, SEEK_END);
    long fsize = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    if (fsize <= 0 ||
        static_cast<unsigned long>(fsize) > MAX_TEST_INPUT_SIZE) {
        fprintf(stderr, "Input must be between 1 and %zu bytes.\n",
                MAX_TEST_INPUT_SIZE);
        fclose(fin);
        return 3;
    }

    const size_t input_size = static_cast<size_t>(fsize);
    uint8_t* mp3_data = new (std::nothrow) uint8_t[input_size];
    if (!mp3_data) { fclose(fin); return 4; }
    size_t bytes_read = fread(mp3_data, 1, input_size, fin);
    const bool read_ok = bytes_read == input_size && ferror(fin) == 0;
    fclose(fin);
    if (!read_ok) {
        fprintf(stderr, "Error: incomplete read from '%s'\n", in_path);
        delete[] mp3_data;
        return 4;
    }

    // ── Allocate output buffer ────────────────────────────────────────
    if (bytes_read > (std::numeric_limits<size_t>::max)() / 24) {
        delete[] mp3_data;
        return 5;
    }
    const size_t out_cap =
        bytes_read * 24 / sizeof(int16_t);
    int16_t* pcm = new (std::nothrow) int16_t[out_cap];
    if (!pcm) { delete[] mp3_data; return 5; }

    std::unique_ptr<madfilter::MadDecoder> decoder(
        new (std::nothrow) madfilter::MadDecoder);
    if (!decoder) {
        delete[] pcm;
        delete[] mp3_data;
        return 5;
    }

    // ── Run test suite ───────────────────────────────────────────────
    if (run_suite) {
        fprintf(stderr, "=== Offset test suite for: %s ===\n", in_path);
        fprintf(stderr, "File size: %zu bytes\n", bytes_read);

        struct TestCase {
            const char* name;
            size_t      start_offset;
            size_t      chunk_size;  // 0 = whole file at once
        };

        TestCase tests[] = {
            {"Byte 0 (normal)",      0,            0},
            {"Byte 1 (off by one)",  1,            0},
            {"Byte 50",              50,           0},
            {"Byte 100",             100,          0},
            {"Byte 500",             500,          0},
            {"Byte 1024",            1024,          0},
            {"1-byte chunks",        0,            1},
            {"7-byte chunks",        0,            7},
            {"13-byte chunks",       0,            13},
            {"100-byte chunks",      0,            100},
            {"Skip 1st frame",       0,            0},  // special: skip first frame
        };

        int passed = 0, failed = 0;
        size_t baseline_samples = 0;

        for (size_t t = 0; t < sizeof(tests)/sizeof(tests[0]); t++) {
            madfilter::maddec_init(decoder.get());

            const uint8_t* start = mp3_data + tests[t].start_offset;
            size_t remaining = bytes_read - tests[t].start_offset;

            // Special: skip first frame
            if (strcmp(tests[t].name, "Skip 1st frame") == 0) {
                uint32_t flen = madfilter::mpeg_frame_length(start);
                if (flen > 0 && flen < remaining) {
                    start += flen;
                    remaining -= flen;
                }
            }

            if (remaining == 0 || remaining > bytes_read) {
                fprintf(stderr, "  SKIP  %-25s (not enough data)\n", tests[t].name);
                madfilter::maddec_finish(decoder.get());
                continue;
            }

            size_t total_samples = decode_file(
                start, remaining, pcm, out_cap, decoder.get(),
                tests[t].chunk_size, false);

            const bool decoder_ok = total_samples != SIZE_MAX;
            if (t == 0 && decoder_ok) {
                baseline_samples = total_samples;
            }

            bool count_matches = true;
            if (tests[t].start_offset == 0 && tests[t].chunk_size > 0) {
                count_matches = total_samples == baseline_samples;
            }

            bool ok = decoder_ok &&
                      (total_samples > 0) &&
                      (decoder->sync_state == madfilter::SyncState::SYNCHRONIZED) &&
                      count_matches;
            fprintf(stderr, "  %s  %-25s → %zu samples, %u Hz %u ch, sync=%s, preroll_discard=%llu\n",
                    ok ? "PASS" : "FAIL",
                    tests[t].name,
                    decoder_ok ? total_samples : 0,
                    decoder->sample_rate, decoder->channels,
                    decoder->sync_state == madfilter::SyncState::SYNCHRONIZED ? "SYNCED" :
                    decoder->sync_state == madfilter::SyncState::SYNCING ? "SYNCING" : "LOST",
                    (unsigned long long)decoder->total_frames_discarded);

            if (ok) passed++; else failed++;
            madfilter::maddec_finish(decoder.get());
        }

        fprintf(stderr, "\n  %d passed, %d failed\n", passed, failed);
        delete[] pcm;
        delete[] mp3_data;
        return failed > 0 ? 1 : 0;
    }

    // ── Single decode mode ────────────────────────────────────────────
    if (offset_param >= bytes_read) {
        fprintf(stderr, "Error: offset %zu exceeds file size %ld\n", offset_param, fsize);
        delete[] pcm;
        delete[] mp3_data;
        return 6;
    }

    madfilter::maddec_init(decoder.get());

    const uint8_t* start = mp3_data + offset_param;
    size_t remaining = bytes_read - offset_param;

    fprintf(stderr, "Decoding from offset %zu, %zu bytes remaining\n", offset_param, remaining);

    if (frag_size != 0) {
        fprintf(stderr, "  Chunk size: %zu bytes\n", frag_size);
    }
    const size_t total_pcm = decode_file(
        start, remaining, pcm, out_cap, decoder.get(), frag_size, true);
    if (total_pcm == SIZE_MAX) {
        madfilter::maddec_finish(decoder.get());
        delete[] pcm;
        delete[] mp3_data;
        return 7;
    }

    fprintf(stderr, "  Decoded: %zu samples, %u Hz %u ch\n",
            total_pcm, decoder->sample_rate, decoder->channels);
    fprintf(stderr, "  Sync state: %s\n",
            decoder->sync_state == madfilter::SyncState::SYNCHRONIZED ? "SYNCHRONIZED" :
            decoder->sync_state == madfilter::SyncState::SYNCING ? "SYNCING" : "LOST");
    fprintf(stderr, "  Frames decoded: %llu, discarded: %llu, sync attempts: %llu\n",
            (unsigned long long)decoder->total_frames_decoded,
            (unsigned long long)decoder->total_frames_discarded,
            (unsigned long long)decoder->total_sync_attempts);

    // ── Write WAV if we got output ────────────────────────────────────
    if (total_pcm > 0 && out_path) {
        FILE* fout = fopen(out_path, "wb");
        if (fout) {
            WavHeader hdr;
            wav_header_init(&hdr, decoder->sample_rate, decoder->channels,
                           (uint32_t)(total_pcm * sizeof(int16_t)));
            const bool header_ok =
                fwrite(&hdr, sizeof(hdr), 1, fout) == 1;
            const bool samples_ok =
                fwrite(pcm, sizeof(int16_t), total_pcm, fout) == total_pcm;
            const bool close_ok = fclose(fout) == 0;
            if (!header_ok || !samples_ok || !close_ok) {
                fprintf(stderr, "Error: failed writing '%s'\n", out_path);
                madfilter::maddec_finish(decoder.get());
                delete[] pcm;
                delete[] mp3_data;
                return 8;
            }
            fprintf(stderr, "  Wrote: %s (%.2f sec)\n",
                    out_path, (double)total_pcm / decoder->channels / decoder->sample_rate);
        } else {
            fprintf(stderr, "Error: cannot create '%s'\n", out_path);
            madfilter::maddec_finish(decoder.get());
            delete[] pcm;
            delete[] mp3_data;
            return 8;
        }
    }

    madfilter::maddec_finish(decoder.get());
    delete[] pcm;
    delete[] mp3_data;
    return 0;
}
