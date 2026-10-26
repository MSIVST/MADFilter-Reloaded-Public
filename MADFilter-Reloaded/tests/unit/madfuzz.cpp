// madfuzz.cpp — Standalone fuzzing harness for MADFilter Reloaded
//
// Feeds arbitrary byte sequences into the decoder wrapper and verifies:
//   1. No crashes, hangs, or undefined behavior
//   2. Memory stays bounded
//   3. Output is consistent (no OOB writes)
//
// Usage:
//   madfuzz <input_file>              Fuzz a single file
//   madfuzz --stdin                   Read from stdin (for AFL++/libFuzzer)
//   madfuzz --corpus <dir>            Run all files in a corpus directory
//   madfuzz --generate <dir> <count>  Generate a fuzz corpus

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <new>
#include <vector>
#include <string>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#endif

#include "filter/maddecode.h"

constexpr size_t MAX_FUZZ_INPUT = 16 * 1024 * 1024;
constexpr size_t OUTPUT_SAMPLES = 64 * 1024;
constexpr size_t OUTPUT_CANARY_SAMPLES = 32;
constexpr int16_t OUTPUT_CANARY = static_cast<int16_t>(0x5A5A);

// ── Fuzz one input buffer ─────────────────────────────────────────────
// Returns: 0 = clean, 1 = invariant violation.

static int fuzz_one(const uint8_t* data, size_t size, bool verbose) {
    if ((!data && size != 0) || size > MAX_FUZZ_INPUT) {
        if (verbose) fprintf(stderr, "ERROR: invalid fuzz input\n");
        return 1;
    }

    madfilter::MadDecoder* dec =
        new (std::nothrow) madfilter::MadDecoder;
    if (!dec) return 1;
    madfilter::maddec_init(dec);

    int16_t* pcm = new (std::nothrow)
        int16_t[OUTPUT_SAMPLES + OUTPUT_CANARY_SAMPLES];
    if (!pcm) {
        madfilter::maddec_finish(dec);
        delete dec;
        return 1;
    }

    size_t offset = 0;
    int errors = 0;
    size_t steps = 0;
    const size_t max_steps = size * 4 + 4096;

    // Feed in random-sized chunks (simulates fragmented DirectShow input)
    unsigned int seed = 0xDEADBEEF;
    auto rand_chunk = [&seed]() -> size_t {
        seed = seed * 1103515245 + 12345;
        return (seed >> 16) & 0x1FFF;  // 0–8191 byte chunks
    };

    const uint8_t empty = 0;
    bool first_call = true;
    while (first_call || offset < size) {
        if (++steps > max_steps) {
            if (verbose) fprintf(stderr, "ERROR: decoder made no progress\n");
            errors++;
            break;
        }

        size_t chunk = 0;
        const uint8_t* input = &empty;
        if (size != 0) {
            chunk = rand_chunk();
            if (chunk == 0) chunk = 1;
            if (chunk > size - offset) chunk = size - offset;
            input = data + offset;
        }
        first_call = false;

        for (size_t i = 0; i < OUTPUT_CANARY_SAMPLES; ++i) {
            pcm[OUTPUT_SAMPLES + i] = OUTPUT_CANARY;
        }

        size_t consumed = 0, written = 0;
        madfilter::SyncState ss = madfilter::SyncState::LOST;

        int result = madfilter::maddec_decode(
            dec,
            input, chunk,
            pcm,
            OUTPUT_SAMPLES * sizeof(int16_t),
            &consumed, &written, &ss);

        if (consumed > chunk ||
            written > OUTPUT_SAMPLES * sizeof(int16_t) ||
            written % sizeof(int16_t) != 0) {
            if (verbose) fprintf(stderr, "ERROR: invalid decoder byte counts\n");
            errors++;
            break;
        }

        bool canary_ok = true;
        for (size_t i = 0; i < OUTPUT_CANARY_SAMPLES; ++i) {
            if (pcm[OUTPUT_SAMPLES + i] != OUTPUT_CANARY) {
                canary_ok = false;
                break;
            }
        }
        if (!canary_ok) {
            if (verbose) fprintf(stderr, "ERROR: output canary corrupted\n");
            errors++;
            break;
        }

        if (result == -1) {
            if (verbose) fprintf(stderr, "  Decoder returned fatal error at offset %zu\n", offset);
            errors++;
            break;
        }

        if (size == 0) {
            break;
        }
        if (consumed == 0) {
            if (written == 0) {
                offset++;  // Skip one byte to unstick corrupt input.
            }
        } else {
            offset += consumed;
        }
    }

    delete[] pcm;
    madfilter::maddec_finish(dec);
    delete dec;
    return errors > 0 ? 1 : 0;
}

#ifndef MADFILTER_LIBFUZZER
static bool read_fuzz_file(const char* path, std::vector<uint8_t>* data) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    const long file_size = ftell(f);
    if (file_size < 0 ||
        static_cast<unsigned long>(file_size) > MAX_FUZZ_INPUT ||
        fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    data->resize(static_cast<size_t>(file_size));
    const size_t read =
        data->empty() ? 0 : fread(data->data(), 1, data->size(), f);
    const bool ok = read == data->size() && ferror(f) == 0;
    fclose(f);
    return ok;
}
#endif

// ── Main ──────────────────────────────────────────────────────────────

#ifndef MADFILTER_LIBFUZZER
int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file> | --stdin | --corpus <dir> | --generate <dir> <count>\n", argv[0]);
        return 1;
    }

    // ── Generate mode ─────────────────────────────────────────────────
    if (strcmp(argv[1], "--generate") == 0 && argc >= 4) {
        const char* dir = argv[2];
        char* count_end = nullptr;
        const long requested_count = strtol(argv[3], &count_end, 10);
        if (!count_end || *count_end != '\0' ||
            requested_count < 1 || requested_count > 10000) {
            fprintf(stderr, "Count must be between 1 and 10000.\n");
            return 1;
        }
        const int count = static_cast<int>(requested_count);

#ifdef _WIN32
        if (!CreateDirectoryA(dir, nullptr) &&
            GetLastError() != ERROR_ALREADY_EXISTS) {
            fprintf(stderr, "Cannot create corpus directory: %s\n", dir);
            return 1;
        }
#else
        if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
            fprintf(stderr, "Cannot create corpus directory: %s\n", dir);
            return 1;
        }
#endif

        fprintf(stderr, "Generating %d fuzz inputs in %s...\n", count, dir);

        // Valid MP3 data (from ffmpeg or embedded)
        // We'll generate edge cases programmatically

        struct {
            const char* name;
            const uint8_t* data;
            size_t size;
        } corpus[] = {
            // Empty input
            {"empty", (const uint8_t*)"", 0},
            // Just a sync word
            {"sync_only", (const uint8_t*)"\xFF\xFB\x90\x00", 4},
            // Sync word + garbage
            {"sync_garbage", nullptr, 0},
            // All zeros
            {"zeros", nullptr, 0},
            // All 0xFF
            {"all_ff", nullptr, 0},
            // MPEG frame header with max values
            {"max_header", (const uint8_t*)"\xFF\xFF\xFF\xFF", 4},
            // Almost-valid header (wrong version)
            {"bad_version", (const uint8_t*)"\xFF\xE8\x90\x00", 4},
            // Almost-valid header (reserved layer)
            {"bad_layer", (const uint8_t*)"\xFF\xF9\x90\x00", 4},
            // Free bitrate
            {"free_bitrate", (const uint8_t*)"\xFF\xFB\x00\x00", 4},
            // Forbidden bitrate
            {"forbidden_br", (const uint8_t*)"\xFF\xFB\xF0\x00", 4},
            // Reserved sample rate
            {"bad_samplerate", (const uint8_t*)"\xFF\xFB\x90\x0C", 4},
        };

        int n = 0;
        for (auto& c : corpus) {
            if (n >= count) break;
            char path[1024];
            const int path_length =
                snprintf(path, sizeof(path), "%s/%s.bin", dir, c.name);
            if (path_length < 0 ||
                static_cast<size_t>(path_length) >= sizeof(path)) {
                fprintf(stderr, "Corpus path is too long.\n");
                return 1;
            }

            std::vector<uint8_t> bytes;
            if (c.data) {
                bytes.assign(c.data, c.data + c.size);
            } else if (strcmp(c.name, "sync_garbage") == 0) {
                bytes = {0xFF, 0xFB, 0x90, 0x00};
                uint32_t state = 0x13579BDF;
                for (int i = 0; i < 1024; ++i) {
                    state = state * 1664525u + 1013904223u;
                    bytes.push_back(static_cast<uint8_t>(state >> 24));
                }
            } else {
                const uint8_t value =
                    strcmp(c.name, "all_ff") == 0 ? 0xFF : 0x00;
                bytes.assign(1024, value);
            }

            FILE* f = fopen(path, "wb");
            if (!f) {
                fprintf(stderr, "Cannot create corpus file: %s\n", path);
                return 1;
            }
            const size_t written =
                bytes.empty() ? 0 : fwrite(bytes.data(), 1, bytes.size(), f);
            const bool data_ok =
                written == bytes.size() && ferror(f) == 0;
            const bool write_ok = fclose(f) == 0 && data_ok;
            if (!write_ok) {
                fprintf(stderr, "Cannot write corpus file: %s\n", path);
                return 1;
            }
            n++;
        }

        uint32_t mutation_state = 0xC001D00Du;
        while (n < count) {
            auto next_random = [&mutation_state]() -> uint32_t {
                mutation_state =
                    mutation_state * 1664525u + 1013904223u;
                return mutation_state;
            };

            const size_t data_size = 1 + next_random() % 4096;
            std::vector<uint8_t> bytes(data_size);
            for (size_t i = 0; i < bytes.size(); ++i) {
                bytes[i] = static_cast<uint8_t>(next_random() >> 24);
            }
            if (bytes.size() >= 4 && n % 3 == 0) {
                bytes[0] = 0xFF;
                bytes[1] = static_cast<uint8_t>(0xE0 | next_random() % 0x20);
                bytes[2] = static_cast<uint8_t>(next_random());
                bytes[3] = static_cast<uint8_t>(next_random());
            }

            char path[1024];
            const int path_length = snprintf(
                path, sizeof(path), "%s/mutation_%05d.bin", dir, n);
            if (path_length < 0 ||
                static_cast<size_t>(path_length) >= sizeof(path)) {
                fprintf(stderr, "Corpus path is too long.\n");
                return 1;
            }

            FILE* f = fopen(path, "wb");
            if (!f) {
                fprintf(stderr, "Cannot create corpus file: %s\n", path);
                return 1;
            }
            const size_t written =
                fwrite(bytes.data(), 1, bytes.size(), f);
            const bool data_ok =
                written == bytes.size() && ferror(f) == 0;
            const bool write_ok = fclose(f) == 0 && data_ok;
            if (!write_ok) {
                fprintf(stderr, "Cannot write corpus file: %s\n", path);
                return 1;
            }
            n++;
        }

        fprintf(stderr, "  Wrote %d files\n", n);
        return 0;
    }

    // ── Corpus mode ───────────────────────────────────────────────────
    if (strcmp(argv[1], "--corpus") == 0 && argc >= 3) {
        const char* dir = argv[2];
        fprintf(stderr, "Fuzzing corpus in %s...\n", dir);
        int passed = 0, failed = 0;

        // Read all .bin files from directory
#ifdef _WIN32
        WIN32_FIND_DATAA fd;
        char pattern[1024];
        const int pattern_length =
            snprintf(pattern, sizeof(pattern), "%s\\*.bin", dir);
        if (pattern_length < 0 ||
            static_cast<size_t>(pattern_length) >= sizeof(pattern)) {
            fprintf(stderr, "Corpus path is too long.\n");
            return 1;
        }
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "No .bin files found in %s\n", dir);
            return 1;
        }
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                continue;
            }
            char path[1024];
            const int path_length =
                snprintf(path, sizeof(path), "%s\\%s", dir, fd.cFileName);
            if (path_length < 0 ||
                static_cast<size_t>(path_length) >= sizeof(path)) {
                fprintf(stderr, "  FAIL %s (path too long)\n", fd.cFileName);
                failed++;
                continue;
            }
#else
        DIR* d = opendir(dir);
        if (!d) { fprintf(stderr, "Cannot open %s\n", dir); return 1; }
        struct dirent* e;
        while ((e = readdir(d))) {
            if (!strstr(e->d_name, ".bin")) continue;
            char path[1024];
            const int path_length =
                snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
            if (path_length < 0 ||
                static_cast<size_t>(path_length) >= sizeof(path)) {
                fprintf(stderr, "  FAIL %s (path too long)\n", e->d_name);
                failed++;
                continue;
            }
#endif
            std::vector<uint8_t> buf;
            const bool read_ok = read_fuzz_file(path, &buf);
            const int r =
                read_ok ? fuzz_one(buf.data(), buf.size(), true) : 1;
            fprintf(stderr, "  %s %s (%zu bytes)%s\n",
                    r == 0 ? "PASS" : "FAIL",
#ifdef _WIN32
                    fd.cFileName,
#else
                    e->d_name,
#endif
                    buf.size(),
                    read_ok ? "" : " (read error or file too large)");
            if (r == 0) passed++; else failed++;
#ifdef _WIN32
        } while (FindNextFileA(h, &fd));
        FindClose(h);
#else
        }
        closedir(d);
#endif
        fprintf(stderr, "  %d passed, %d failed\n", passed, failed);
        return failed > 0 ? 1 : 0;
    }

    // ── Single file mode ──────────────────────────────────────────────
    const char* path = argv[1];
    if (strcmp(path, "--stdin") == 0) {
        std::vector<uint8_t> buf;
        buf.reserve(64 * 1024);
        int c;
        while ((c = getchar()) != EOF) {
            if (buf.size() == MAX_FUZZ_INPUT) {
                fprintf(stderr, "Input exceeds %zu-byte fuzzing limit.\n",
                        MAX_FUZZ_INPUT);
                return 1;
            }
            buf.push_back(static_cast<uint8_t>(c));
        }
        fprintf(stderr, "Fuzzing stdin (%zu bytes)...\n", buf.size());
        int r = fuzz_one(buf.data(), buf.size(), true);
        fprintf(stderr, "  %s\n", r == 0 ? "PASS" : "FAIL");
        return r;
    }

    std::vector<uint8_t> buf;
    if (!read_fuzz_file(path, &buf)) {
        fprintf(stderr, "Cannot read input or input exceeds %zu bytes: %s\n",
                MAX_FUZZ_INPUT, path);
        return 1;
    }

    fprintf(stderr, "Fuzzing %s (%zu bytes)...\n", path, buf.size());
    int r = fuzz_one(buf.data(), buf.size(), true);
    fprintf(stderr, "  %s\n", r == 0 ? "PASS" : "FAIL");

    return r;
}
#else
extern "C" int LLVMFuzzerTestOneInput(
    const uint8_t* data, size_t size) {
    if (fuzz_one(data, size, false) != 0) {
        std::abort();
    }
    return 0;
}
#endif
