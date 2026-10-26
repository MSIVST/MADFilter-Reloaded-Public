// MADFilterReloaded.cpp — DirectShow MPEG audio transform filter (impl)
//
// 32-bit x86 DirectShow transform filter using Tenacity libmad.
// Based on the original MADFilter by Armin Gerritsen (2002),
// reworked for security, correctness, and compatibility.
//
// Key changes from the original:
//   - New CLSID (no conflict with original)
//   - MERIT_NORMAL instead of MERIT_PREFERRED + 0x2E90000
//   - Instance-based decoder state (no globals)
//   - Bounded internal buffering
//   - Proper flush / discontinuity / EndOfStream handling
//   - Checked media-type validation
//   - /GS, /SAFESEH, /NXCOMPAT, /DYNAMICBASE compatible

#include "MADFilterReloaded.h"

#include <mmreg.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <new>

// ── Constants ─────────────────────────────────────────────────────────

// Default input sample size; used for buffer allocation estimate.
constexpr LONG DEFAULT_INPUT_SIZE = 8000;
// One MPEG frame can produce at most 1152 samples x 2 channels x 2 bytes.
constexpr LONG MIN_OUTPUT_SIZE =
    static_cast<LONG>(madfilter::MAX_PCM_S16_PER_FRAME * sizeof(int16_t));
// Keep enough room for the bounded per-call frame budget.  Without this,
// low-rate streams can fill the output after one small frame and force the
// compressed carry to grow across input samples.
constexpr LONG OUTPUT_BUFFER_FLOOR = 192 * 1024;
// Upper bound for downstream allocator proposals.
constexpr LONG MAX_OUTPUT_SIZE = 256 * 1024;

// A flushed segment whose start timestamp is within this window of the
// decoder timeline is treated as a continuation of the same compressed
// stream (Fallout 3 radio delivers contiguous chunks this way).
// BSAudio radio advances segment starts by seconds without ending the
// stream.  Generic DirectShow sources use ordinary seek semantics.
constexpr REFERENCE_TIME MAX_BSAUDIO_CONTINUATION =
    600LL * 10000000; // 10 min

constexpr LONGLONG REFERENCE_TIME_PER_SECOND = 10000000;
constexpr DWORD FALLOUT3_MERIT = MERIT_PREFERRED + 1;

static REFERENCE_TIME mad_timer_to_reference_time(mad_timer_t timer) {
    const LONGLONG whole =
        static_cast<LONGLONG>(timer.seconds) * REFERENCE_TIME_PER_SECOND;
    const LONGLONG fraction =
        (static_cast<LONGLONG>(timer.fraction) *
             REFERENCE_TIME_PER_SECOND +
         MAD_TIMER_RESOLUTION / 2) /
        MAD_TIMER_RESOLUTION;
    return static_cast<REFERENCE_TIME>(whole + fraction);
}

// The MP3 subtype GUID is not always defined in older SDKs.
// WAVE_FORMAT_MPEGLAYER3 = 0x0055
#ifndef WAVE_FORMAT_MPEGLAYER3
#define WAVE_FORMAT_MPEGLAYER3 0x0055
#endif
#ifndef WAVE_FORMAT_MPEG
#define WAVE_FORMAT_MPEG 0x0050
#endif

DEFINE_GUID(MEDIASUBTYPE_MP3,
    WAVE_FORMAT_MPEGLAYER3, 0x0000, 0x0010, 0x80, 0x00,
    0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

// ── Format helpers ────────────────────────────────────────────────────

static bool TryGetWaveFormat(const CMediaType* pmt, WAVEFORMATEX* out) {
    if (!pmt || !out) return false;
    if (*pmt->FormatType() != FORMAT_WaveFormatEx) return false;
    if (!pmt->Format()) return false;
    if (pmt->FormatLength() < sizeof(WAVEFORMATEX)) return false;

    const WAVEFORMATEX* wfx =
        reinterpret_cast<const WAVEFORMATEX*>(pmt->Format());
    if (wfx->cbSize > pmt->FormatLength() - sizeof(WAVEFORMATEX)) {
        return false;
    }
    *out = *wfx;
    return true;
}

static bool IsSupportedSampleRate(DWORD rate) {
    switch (rate) {
    case 48000:
    case 44100:
    case 32000:
    case 24000:
    case 22050:
    case 16000:
    case 12000:
    case 11025:
    case 8000:
        return true;
    default:
        return false;
    }
}

static bool IsMpegWaveFormatTag(WORD tag) {
    return tag == 0 ||
           tag == WAVE_FORMAT_MPEG ||
           tag == WAVE_FORMAT_MPEGLAYER3;
}

static void SetPcmWaveFormat(WAVEFORMATEX* wfx, DWORD rate, WORD channels) {
    std::memset(wfx, 0, sizeof(*wfx));
    wfx->wFormatTag      = WAVE_FORMAT_PCM;
    wfx->nChannels       = channels;
    wfx->nSamplesPerSec  = rate;
    wfx->wBitsPerSample  = 16;
    wfx->nBlockAlign     = static_cast<WORD>(channels * sizeof(int16_t));
    wfx->nAvgBytesPerSec = rate * wfx->nBlockAlign;
    wfx->cbSize          = 0;
}

static bool IsBethesdaBsaAudioHost() {
    WCHAR path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(
        nullptr, path, _countof(path));
    if (length == 0 || length >= _countof(path)) {
        return false;
    }

    const WCHAR* name = wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    return lstrcmpiW(name, L"Fallout3.exe") == 0 ||
           lstrcmpiW(name, L"FalloutNV.exe") == 0 ||
           lstrcmpiW(name, L"Oblivion.exe") == 0;
}

// ── Registration data ─────────────────────────────────────────────────
// AMOVIESETUP_MEDIATYPE and AMOVIESETUP_PIN / AMOVIESETUP_FILTER
// are part of the DirectShow self-registration infrastructure.

const AMOVIESETUP_MEDIATYPE sudInputTypes[] = {
    { &MEDIATYPE_Audio,          &MEDIASUBTYPE_MP3              },
    { &MEDIATYPE_Audio,          &MEDIASUBTYPE_MPEG1Audio       },
    { &MEDIATYPE_Audio,          &MEDIASUBTYPE_MPEG1AudioPayload},
};

const AMOVIESETUP_MEDIATYPE sudOutputTypes[] = {
    { &MEDIATYPE_Audio,          &MEDIASUBTYPE_PCM              },
};

const AMOVIESETUP_PIN sudPins[] = {
    {
        L"Input",                   // Pin name
        FALSE,                      // Not a renderer
        FALSE,                      // Not an output pin
        FALSE,                      // No fixed instance count
        FALSE,                      // No fixed instance count
        nullptr,                    // No connected CLSID restriction
        nullptr,                    // No connected pin name restriction
        _countof(sudInputTypes),    // Number of media types
        sudInputTypes               // Accepted media types
    },
    {
        L"Output",                  // Pin name
        FALSE,                      // Not a renderer
        TRUE,                       // IS an output pin
        FALSE,                      // No fixed instance count
        FALSE,                      // No fixed instance count
        nullptr,                    // No connected CLSID restriction
        nullptr,                    // No connected pin name restriction
        _countof(sudOutputTypes),   // Number of media types
        sudOutputTypes              // Offered media types
    }
};

AMOVIESETUP_FILTER sudMADFilterReloaded = {
    &CLSID_MADFilterReloaded,       // CLSID
    MADFILTER_RELOADED_NAME,        // Friendly name
    MERIT_NORMAL,                   // Filter merit — CONSERVATIVE (not like original!)
    _countof(sudPins),              // Number of pins
    sudPins                         // Pin info
};

// ── Class factory template ────────────────────────────────────────────

CFactoryTemplate g_Templates[] = {
    {
        MADFILTER_RELOADED_NAME,
        &CLSID_MADFilterReloaded,
        CMADFilterReloaded::CreateInstance,
        nullptr,
        &sudMADFilterReloaded
    }
};
int g_cTemplates = _countof(g_Templates);

// ── COM registration ──────────────────────────────────────────────────

static HRESULT RegisterServerWithMerit(DWORD merit) {
    const DWORD previous_merit = sudMADFilterReloaded.dwMerit;
    sudMADFilterReloaded.dwMerit = merit;
    const HRESULT result = AMovieDllRegisterServer2(TRUE);
    sudMADFilterReloaded.dwMerit = previous_merit;
    return result;
}

static bool EqualsAsciiCaseInsensitive(LPCWSTR left, LPCWSTR right) {
    if (!left || !right) {
        return false;
    }

    while (*left != L'\0' && *right != L'\0') {
        wchar_t left_char = *left++;
        wchar_t right_char = *right++;
        if (left_char >= L'A' && left_char <= L'Z') {
            left_char += L'a' - L'A';
        }
        if (right_char >= L'A' && right_char <= L'Z') {
            right_char += L'a' - L'A';
        }
        if (left_char != right_char) {
            return false;
        }
    }

    return *left == *right;
}

STDAPI DllRegisterServer() {
    return RegisterServerWithMerit(MERIT_NORMAL);
}

STDAPI DllUnregisterServer() {
    return AMovieDllRegisterServer2(FALSE);
}

STDAPI DllInstall(BOOL install, LPCWSTR command_line) {
    if (!install) {
        return DllUnregisterServer();
    }

    if (command_line && *command_line != L'\0') {
        if (!EqualsAsciiCaseInsensitive(command_line, L"fallout3")) {
            return E_INVALIDARG;
        }
        return RegisterServerWithMerit(FALLOUT3_MERIT);
    }

    return RegisterServerWithMerit(MERIT_NORMAL);
}

// ── DLL entry point ───────────────────────────────────────────────────

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved) {
    return DllEntryPoint(reinterpret_cast<HINSTANCE>(hDllHandle),
                         dwReason, lpReserved);
}

// ═══════════════════════════════════════════════════════════════════════
// CMADFilterReloaded implementation
// ═══════════════════════════════════════════════════════════════════════

// ── CreateInstance (COM factory) ──────────────────────────────────────

CUnknown* WINAPI CMADFilterReloaded::CreateInstance(LPUNKNOWN punk, HRESULT* phr) {
    CMADFilterReloaded* pFilter = new (std::nothrow) CMADFilterReloaded(punk, phr);
    if (!pFilter) {
        if (phr) *phr = E_OUTOFMEMORY;
    }
    return pFilter;
}

// ── Constructor ───────────────────────────────────────────────────────

CMADFilterReloaded::CMADFilterReloaded(LPUNKNOWN punk, HRESULT* phr)
    : CTransformFilter(NAME("MADFilter Reloaded"), punk, CLSID_MADFilterReloaded)
    , m_last_sync_state(madfilter::SyncState::LOST)
    , m_last_decode_errors(0)
    , m_last_concealed_frames(0)
    , m_last_discarded_frames(0)
    , m_last_sample_rate(0)
    , m_last_channels(0)
    , m_last_scan_exhausted(false)
    , m_last_carry_len(0)
    , m_total_pcm_bytes(0)
    , m_received_samples(0)
    , m_received_bytes(0)
    , m_nonmedia_samples(0)
    , m_first_input{}
    , m_first_input_len(0)
    , m_first_output_time(0)
    , m_last_output_time(0)
    , m_have_output_time(false)
    , m_force_reset(false)
    , m_segment_pending(false)
    , m_have_last_segment_start(false)
    , m_segment_start(0)
    , m_last_segment_start(0)
    , m_output_format_set(false)
    , m_emit_timestamps(true)
    , m_bsaudio_mode(false)
    , m_source_checked(false)
    , m_input_format_valid(false)
    , m_format_mismatch_reported(false)
    , m_discontinuity_pending(true)
    , m_stream_time_base(0)
    , m_stream_time_base_valid(false)
    , m_input_carry_len(0)
{
    WCHAR env_value[8] = {};
    const bool force_no_timestamps =
        GetEnvironmentVariableW(
            L"MADFILTER_NO_TIMESTAMPS", env_value,
            _countof(env_value)) != 0;
    const bool force_bsaudio =
        GetEnvironmentVariableW(
            L"MADFILTER_BSAUDIO", env_value,
            _countof(env_value)) != 0;
    const bool host_bsaudio = IsBethesdaBsaAudioHost();
    m_bsaudio_mode = host_bsaudio || force_bsaudio;
    m_emit_timestamps = !(force_no_timestamps || m_bsaudio_mode);
    m_diag.record(
        madfilter::DIAG_POLICY,
        m_emit_timestamps ? 1u : 0u,
        m_bsaudio_mode ? 1u : 0u);

    // Initialize decoder state
    madfilter::maddec_init(&m_decoder);
    m_decoder.initial_preroll_frames =
        m_bsaudio_mode ? 0 : madfilter::PREROLL_FRAMES;
    ResetDiagnosticBaseline();

    // Clear format state
    std::memset(&m_input_format, 0, sizeof(m_input_format));
    std::memset(&m_output_format, 0, sizeof(m_output_format));

    if (phr) *phr = S_OK;
}

// ── Destructor ────────────────────────────────────────────────────────

CMADFilterReloaded::~CMADFilterReloaded() {
    DumpDiagnostics("destructor");
    madfilter::maddec_finish(&m_decoder);
}

void CMADFilterReloaded::ClearInputCarry() {
    m_input_carry_len = 0;
}

void CMADFilterReloaded::ResetDiagnosticBaseline() {
    m_last_sync_state = madfilter::SyncState::LOST;
    m_last_decode_errors = 0;
    m_last_concealed_frames = 0;
    m_last_discarded_frames = 0;
    m_last_sample_rate = 0;
    m_last_channels = 0;
    m_last_scan_exhausted = false;
    m_last_carry_len = m_input_carry_len;
}

static const char* diag_code_name(uint32_t code) {
    switch (code) {
    case madfilter::DIAG_POLICY:          return "policy";
    case madfilter::DIAG_SOURCE_BSA:      return "source-bsa";
    case madfilter::DIAG_SOURCE_GENERIC:  return "source-generic";
    case madfilter::DIAG_SOURCE_BYTE_CLOCK:
        return "source-byte-clock";
    case madfilter::DIAG_INPUT_FORMAT:   return "input-format";
    case madfilter::DIAG_OUTPUT_FORMAT:  return "output-format";
    case madfilter::DIAG_SEGMENT:        return "segment";
    case madfilter::DIAG_SEGMENT_RESET:  return "segment-reset";
    case madfilter::DIAG_CONTINUATION:   return "continuation";
    case madfilter::DIAG_INPUT_TIMESTAMP:return "input-timestamp";
    case madfilter::DIAG_INPUT_DISCONTINUITY:
        return "input-discontinuity";
    case madfilter::DIAG_FLUSH_BEGIN:    return "flush-begin";
    case madfilter::DIAG_FLUSH_END:      return "flush-end";
    case madfilter::DIAG_SYNC_ACQUIRED:  return "sync-acquired";
    case madfilter::DIAG_SYNC_LOST:      return "sync-lost";
    case madfilter::DIAG_SCAN_EXHAUSTED: return "scan-exhausted";
    case madfilter::DIAG_PREROLL_DONE:   return "preroll-done";
    case madfilter::DIAG_DECODED_FORMAT: return "decoded-format";
    case madfilter::DIAG_DECODE_ERROR:   return "decode-error";
    case madfilter::DIAG_CONCEAL:        return "conceal";
    case madfilter::DIAG_FORMAT_MISMATCH:return "format-mismatch";
    case madfilter::DIAG_CARRY:          return "carry";
    case madfilter::DIAG_CARRY_DRAINED:  return "carry-drained";
    case madfilter::DIAG_UNDERFLOW:      return "underflow";
    case madfilter::DIAG_RECEIVE_ERROR:  return "receive-error";
    default:                             return "unknown";
    }
}

void CMADFilterReloaded::DumpDiagnostics(const char* reason) const {
    WCHAR path[1024];
    const DWORD length = GetEnvironmentVariableW(
        L"MADFILTER_DIAG_LOG", path, _countof(path));
    if (length == 0 || length >= _countof(path)) {
        return;
    }

    HANDLE file = CreateFileW(
        path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
        OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    auto write_line = [file](const char* text) {
        DWORD written = 0;
        if (text && *text) {
            WriteFile(
                file, text,
                static_cast<DWORD>(std::strlen(text)), &written, nullptr);
        }
        WriteFile(file, "\r\n", 2, &written, nullptr);
    };

    char line[512];
    SYSTEMTIME now;
    GetLocalTime(&now);
    std::snprintf(
        line, sizeof(line),
        "---- %04u-%02u-%02u %02u:%02u:%02u.%03u tick=%lu reason=%s instance=%p ----",
        now.wYear, now.wMonth, now.wDay,
        now.wHour, now.wMinute, now.wSecond, now.wMilliseconds,
        static_cast<unsigned long>(GetTickCount()),
        reason ? reason : "", static_cast<const void*>(this));
    write_line(line);

    std::snprintf(
        line, sizeof(line),
        "input=%lu Hz %u ch valid=%d | output=%lu Hz %u ch set=%d | "
        "sync=%d | carry=%zu",
        static_cast<unsigned long>(m_input_format.nSamplesPerSec),
        static_cast<unsigned>(m_input_format.nChannels),
        m_input_format_valid ? 1 : 0,
        static_cast<unsigned long>(m_output_format.nSamplesPerSec),
        static_cast<unsigned>(m_output_format.nChannels),
        m_output_format_set ? 1 : 0,
        static_cast<int>(m_decoder.sync_state),
        m_input_carry_len);
    write_line(line);

    std::snprintf(
        line, sizeof(line),
        "frames=%llu discarded=%llu sync_attempts=%llu concealed=%llu "
        "decode_errors=%llu last_error=0x%08lX scan_exhausted=%d "
        "pcm_bytes=%llu first_out=%lld last_out=%lld",
        static_cast<unsigned long long>(m_decoder.total_frames_decoded),
        static_cast<unsigned long long>(m_decoder.total_frames_discarded),
        static_cast<unsigned long long>(m_decoder.total_sync_attempts),
        static_cast<unsigned long long>(m_decoder.total_concealed_frames),
        static_cast<unsigned long long>(m_decoder.total_decode_errors),
        static_cast<unsigned long>(m_decoder.last_error),
        m_decoder.scan_exhausted ? 1 : 0,
        static_cast<unsigned long long>(m_total_pcm_bytes),
        m_have_output_time ? m_first_output_time : 0,
        m_have_output_time ? m_last_output_time : 0);
    write_line(line);

    char input_hex[sizeof(m_first_input) * 2 + 1] = {};
    for (size_t i = 0; i < m_first_input_len; ++i) {
        std::snprintf(input_hex + i * 2, 3, "%02x",
                      static_cast<unsigned>(m_first_input[i]));
    }
    std::snprintf(
        line, sizeof(line),
        "received_samples=%llu received_bytes=%llu nonmedia_samples=%llu first_input=%s",
        static_cast<unsigned long long>(m_received_samples),
        static_cast<unsigned long long>(m_received_bytes),
        static_cast<unsigned long long>(m_nonmedia_samples), input_hex);
    write_line(line);

    for (size_t i = 0; i < m_diag.size(); ++i) {
        const madfilter::DiagEvent& event = m_diag.at(i);
        std::snprintf(
            line, sizeof(line), "[%lu] %s a=%lu b=%lu",
            static_cast<unsigned long>(event.tick),
            diag_code_name(event.code),
            static_cast<unsigned long>(event.a),
            static_cast<unsigned long>(event.b));
        write_line(line);
    }

    CloseHandle(file);
}

void CMADFilterReloaded::UpdateDiagnostics(
    madfilter::SyncState sync_state)
{
    if (sync_state != m_last_sync_state) {
        if (sync_state == madfilter::SyncState::SYNCHRONIZED) {
            m_diag.record(
                madfilter::DIAG_SYNC_ACQUIRED,
                m_decoder.sample_rate,
                m_decoder.channels);
#ifdef _DEBUG
            OutputDebugStringA("MADFilter Reloaded: synchronized\n");
#endif
        } else if (sync_state == madfilter::SyncState::LOST &&
                   m_last_sync_state ==
                       madfilter::SyncState::SYNCHRONIZED) {
            m_diag.record(madfilter::DIAG_SYNC_LOST);
#ifdef _DEBUG
            OutputDebugStringA("MADFilter Reloaded: lost sync\n");
#endif
        }
        m_last_sync_state = sync_state;
    }

    if (m_decoder.total_decode_errors != m_last_decode_errors) {
        const uint64_t delta =
            m_decoder.total_decode_errors - m_last_decode_errors;
        m_diag.record(
            madfilter::DIAG_DECODE_ERROR,
            m_decoder.last_error,
            static_cast<uint32_t>(delta));
        m_last_decode_errors = m_decoder.total_decode_errors;
    }

    if (m_decoder.total_concealed_frames != m_last_concealed_frames) {
        const uint64_t delta =
            m_decoder.total_concealed_frames - m_last_concealed_frames;
        m_diag.record(
            madfilter::DIAG_CONCEAL,
            static_cast<uint32_t>(delta));
        m_last_concealed_frames = m_decoder.total_concealed_frames;
    }

    if (m_decoder.total_frames_discarded != m_last_discarded_frames) {
        m_last_discarded_frames = m_decoder.total_frames_discarded;
        if (sync_state == madfilter::SyncState::SYNCHRONIZED) {
            m_diag.record(
                madfilter::DIAG_PREROLL_DONE,
                static_cast<uint32_t>(m_last_discarded_frames));
        }
    }

    if (m_decoder.sample_rate != m_last_sample_rate ||
        m_decoder.channels != m_last_channels) {
        m_diag.record(
            madfilter::DIAG_DECODED_FORMAT,
            m_decoder.sample_rate,
            m_decoder.channels);
        m_last_sample_rate = m_decoder.sample_rate;
        m_last_channels = m_decoder.channels;
    }

    if (m_decoder.scan_exhausted && !m_last_scan_exhausted) {
        m_diag.record(madfilter::DIAG_SCAN_EXHAUSTED);
#ifdef _DEBUG
        OutputDebugStringA("MADFilter Reloaded: scan budget exhausted\n");
#endif
    }
    m_last_scan_exhausted = m_decoder.scan_exhausted;

    if (m_input_carry_len != m_last_carry_len) {
        if (m_input_carry_len > 0 && m_last_carry_len == 0) {
            m_diag.record(
                madfilter::DIAG_CARRY,
                static_cast<uint32_t>(m_input_carry_len));
        } else if (m_input_carry_len == 0 && m_last_carry_len > 0) {
            m_diag.record(
                madfilter::DIAG_CARRY_DRAINED,
                static_cast<uint32_t>(m_last_carry_len));
        }
        m_last_carry_len = m_input_carry_len;
    }
}

HRESULT CMADFilterReloaded::DecodeInput(
    const uint8_t* data, size_t length,
    BYTE* out, size_t out_capacity,
    size_t* consumed, size_t* written,
    madfilter::SyncState* sync_state, size_t max_frames)
{
    *consumed = 0;
    *written  = 0;

    constexpr size_t kMaxFrameBytes =
        madfilter::MAX_PCM_S16_PER_FRAME * sizeof(int16_t);
    const uint64_t start_frames = m_decoder.total_frames_decoded;
    size_t total_consumed = 0;
    size_t total_written  = 0;

    while (total_consumed < length && total_written < out_capacity) {
        const size_t available = out_capacity - total_written;
        if (available < kMaxFrameBytes) {
            break;
        }

        const uint64_t decoded =
            m_decoder.total_frames_decoded - start_frames;
        if (decoded >= max_frames) {
            break;
        }
        const size_t frames_left = static_cast<size_t>(
            max_frames - decoded);

        size_t chunk_consumed = 0;
        size_t chunk_written  = 0;
        const int result = madfilter::maddec_decode(
            &m_decoder,
            data + total_consumed,
            length - total_consumed,
            reinterpret_cast<int16_t*>(out + total_written),
            available,
            &chunk_consumed,
            &chunk_written,
            sync_state, frames_left);

        if (result == -1) {
            return E_FAIL;
        }
        if (chunk_consumed > length - total_consumed ||
            chunk_written > out_capacity - total_written) {
            return E_UNEXPECTED;
        }

        total_consumed += chunk_consumed;
        total_written  += chunk_written;

        if (chunk_consumed == 0 && chunk_written == 0) {
            break;
        }
    }

    *consumed = total_consumed;
    *written  = total_written;
    return S_OK;
}

HRESULT CMADFilterReloaded::DrainInputCarry(
    BYTE* out, size_t out_capacity,
    size_t* written,
    madfilter::SyncState* sync_state, size_t max_frames)
{
    *written = 0;
    if (m_input_carry_len == 0) {
        return S_OK;
    }

    size_t consumed = 0;
    size_t produced = 0;
    const HRESULT hr = DecodeInput(
        m_input_carry, m_input_carry_len,
        out, out_capacity,
        &consumed, &produced, sync_state, max_frames);
    if (FAILED(hr)) {
        return hr;
    }

    if (consumed > 0) {
        const size_t remaining = m_input_carry_len - consumed;
        if (remaining > 0) {
            std::memmove(m_input_carry, m_input_carry + consumed, remaining);
        }
        m_input_carry_len = remaining;
    }

    *written = produced;
    return S_OK;
}

void CMADFilterReloaded::UpdateSourcePolicy() {
    if (m_source_checked) return;
    m_source_checked = true;

    // Host names and the explicit environment override select the policy.
    // Avoid walking the upstream graph on the streaming thread.
    if (m_bsaudio_mode) {
        m_diag.record(madfilter::DIAG_SOURCE_BSA, 2, 0);
    } else {
        m_diag.record(madfilter::DIAG_SOURCE_GENERIC, 0, 0);
    }
}

HRESULT CMADFilterReloaded::DecodeExtraOutput(
    BYTE* out, size_t out_capacity,
    size_t* written,
    madfilter::SyncState* sync_state, size_t max_frames)
{
    *written = 0;
    if (m_input_carry_len > 0) {
        return DrainInputCarry(
            out, out_capacity, written, sync_state, max_frames);
    }

    if (m_decoder.ibuf_len == 0) {
        return S_OK;
    }

    size_t consumed = 0;
    const int result = madfilter::maddec_decode_buffered(
        &m_decoder,
        reinterpret_cast<int16_t*>(out),
        out_capacity,
        &consumed,
        written,
        sync_state, max_frames);
    if (result == -1 || consumed != 0) {
        return E_FAIL;
    }
    return S_OK;
}

HRESULT CMADFilterReloaded::Receive(IMediaSample* pIn) {
    CheckPointer(pIn, E_POINTER);
    ++m_received_samples;
    const LONG input_length = pIn->GetActualDataLength();
    if (input_length > 0) {
        m_received_bytes += static_cast<uint64_t>(input_length);
    }
    if (!m_pInput || !m_pOutput) {
        return CTransformFilter::Receive(pIn);
    }

    AM_SAMPLE2_PROPERTIES* props = m_pInput->SampleProps();
    if (!props || props->dwStreamId != AM_STREAM_MEDIA) {
        ++m_nonmedia_samples;
        return CTransformFilter::Receive(pIn);
    }

    // Deliver the normal first output sample (Transform handles all input
    // accounting, carry, timestamps, and discontinuity state).
    HRESULT hr = CTransformFilter::Receive(pIn);
    if (FAILED(hr)) {
        m_diag.record(madfilter::DIAG_RECEIVE_ERROR, static_cast<uint32_t>(hr));
    }
    if (FAILED(hr) || !m_pOutput->IsConnected()) {
        return hr;
    }

    // BSAudio feeds large read-ahead chunks.  Drain only a real compressed
    // carry here; short voice clips keep the baseline one-output-per-input
    // behavior.
    const uint64_t start_frames = m_decoder.total_frames_decoded;
    for (size_t i = 0; i < MAX_EXTRA_OUTPUTS_PER_RECEIVE; ++i) {
        // Only drain a real compressed carry.  Short voice clips and
        // ordinary samples keep the baseline one-output-per-input
        // behavior, which avoids over-delivering into BSAudio graphs.
        if (m_input_carry_len == 0) {
            break;
        }
        if (m_decoder.total_frames_decoded - start_frames >=
            MAX_EXTRA_DECODE_FRAMES) {
            break;
        }

        IMediaSample* out = nullptr;
        hr = m_pOutput->GetDeliveryBuffer(&out, nullptr, nullptr, 0);
        if (FAILED(hr)) {
            return hr;
        }

        BYTE* data = nullptr;
        hr = out->GetPointer(&data);
        if (FAILED(hr) || !data || out->GetSize() < MIN_OUTPUT_SIZE) {
            out->Release();
            break;
        }

        size_t written = 0;
        madfilter::SyncState sync_state = m_decoder.sync_state;
        hr = DecodeExtraOutput(
            data, static_cast<size_t>(out->GetSize()),
            &written, &sync_state,
            static_cast<size_t>(MAX_EXTRA_DECODE_FRAMES -
                (m_decoder.total_frames_decoded - start_frames)));
        if (FAILED(hr)) {
            out->Release();
            return hr;
        }
        if (written == 0) {
            out->Release();
            break;
        }

        UpdateDiagnostics(m_decoder.sync_state);
        out->SetActualDataLength(static_cast<LONG>(written));
        hr = SetOutputTimestamp(out, written);
        if (FAILED(hr)) {
            out->Release();
            return hr;
        }

        m_total_pcm_bytes += written;
        REFERENCE_TIME out_start = 0;
        REFERENCE_TIME out_end = 0;
        if (out->GetTime(&out_start, &out_end) == S_OK) {
            if (!m_have_output_time) {
                m_first_output_time = out_start;
                m_have_output_time = true;
            }
            m_last_output_time = out_end;
        }

        out->SetMediaTime(nullptr, nullptr);
        out->SetSyncPoint(FALSE);
        out->SetPreroll(FALSE);
        out->SetDiscontinuity(FALSE);
        hr = m_pOutput->Deliver(out);
        out->Release();
        if (FAILED(hr)) {
            return hr;
        }
    }

    return S_OK;
}

HRESULT CMADFilterReloaded::SetMediaType(
    PIN_DIRECTION direction, const CMediaType* pmt)
{
    const HRESULT hr = CTransformFilter::SetMediaType(direction, pmt);
    if (FAILED(hr)) {
        return hr;
    }

    WAVEFORMATEX wfx;
    const bool has_wfx = TryGetWaveFormat(pmt, &wfx);

    if (direction == PINDIR_INPUT) {
        m_input_format_valid =
            has_wfx &&
            wfx.nChannels >= 1 && wfx.nChannels <= 2 &&
            IsSupportedSampleRate(wfx.nSamplesPerSec);
        if (m_input_format_valid) {
            m_input_format = wfx;
            m_diag.record(
                madfilter::DIAG_INPUT_FORMAT,
                wfx.nSamplesPerSec,
                wfx.nChannels);
        } else {
            std::memset(&m_input_format, 0, sizeof(m_input_format));
        }
    } else if (direction == PINDIR_OUTPUT) {
        const bool valid_pcm =
            has_wfx &&
            wfx.wFormatTag == WAVE_FORMAT_PCM &&
            wfx.wBitsPerSample == 16 &&
            wfx.nChannels >= 1 && wfx.nChannels <= 2 &&
            IsSupportedSampleRate(wfx.nSamplesPerSec) &&
            wfx.nBlockAlign == wfx.nChannels * sizeof(int16_t);
        if (valid_pcm) {
            m_output_format = wfx;
            m_output_format_set = true;
            m_diag.record(
                madfilter::DIAG_OUTPUT_FORMAT,
                wfx.nSamplesPerSec,
                wfx.nChannels);
        }
    }

    return hr;
}

HRESULT CMADFilterReloaded::SetOutputTimestamp(
    IMediaSample* sample, size_t pcm_bytes)
{
    // Shared by normal delivery, extra carry output, and EOF draining.
    const HRESULT format_hr = ValidateOutputFormat();
    if (FAILED(format_hr)) return format_hr;
    if (!m_emit_timestamps ||
        !m_stream_time_base_valid ||
        m_decoder.sample_rate == 0 ||
        m_decoder.channels == 0) {
        return sample->SetTime(nullptr, nullptr);
    }

    const size_t block_align =
        static_cast<size_t>(m_decoder.channels) * sizeof(int16_t);
    if (pcm_bytes % block_align != 0) {
        return E_UNEXPECTED;
    }

    const unsigned long sample_count =
        static_cast<unsigned long>(pcm_bytes / block_align);
    mad_timer_t start_timer = m_decoder.timer;
    mad_timer_t duration;
    mad_timer_set(
        &duration, 0, sample_count, m_decoder.sample_rate);
    mad_timer_negate(&duration);
    mad_timer_add(&start_timer, duration);

    REFERENCE_TIME end =
        m_stream_time_base + mad_timer_to_reference_time(m_decoder.timer);
    REFERENCE_TIME start =
        m_stream_time_base + mad_timer_to_reference_time(start_timer);
    return sample->SetTime(&start, &end);
}

// ── NonDelegatingQueryInterface ───────────────────────────────────────

STDMETHODIMP CMADFilterReloaded::NonDelegatingQueryInterface(REFIID riid, void** ppv) {
    CheckPointer(ppv, E_POINTER);
    // For now, only expose IUnknown (via base class).
    // Future: re-add ISpecifyPropertyPages and IMADFilterInterface.
    return CTransformFilter::NonDelegatingQueryInterface(riid, ppv);
}

// ── CheckInputType ────────────────────────────────────────────────────
// Validate that the upstream media type is a supported MPEG audio format.

HRESULT CMADFilterReloaded::CheckInputType(const CMediaType* pmt) {
    CheckPointer(pmt, E_POINTER);

    // Must be audio
    if (*pmt->Type() != MEDIATYPE_Audio)
        return VFW_E_TYPE_NOT_ACCEPTED;

    // Must be a supported subtype
    const GUID& subtype = *pmt->Subtype();
    if (subtype != MEDIASUBTYPE_MPEG1Audio       &&
        subtype != MEDIASUBTYPE_MPEG1AudioPayload &&
        subtype != MEDIASUBTYPE_MP3)
    {
        // MEDIASUBTYPE_NULL was accepted by the original but we are stricter.
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    // A missing format is allowed (the decoder will probe the first frame),
    // but a present one must be internally consistent.
    if (*pmt->FormatType() == FORMAT_WaveFormatEx) {
        WAVEFORMATEX wfx;
        if (!TryGetWaveFormat(pmt, &wfx))
            return VFW_E_TYPE_NOT_ACCEPTED;
        if (!IsMpegWaveFormatTag(wfx.wFormatTag))
            return VFW_E_TYPE_NOT_ACCEPTED;
        if (wfx.nChannels > 2)
            return VFW_E_TYPE_NOT_ACCEPTED;
        if (wfx.wBitsPerSample != 0 && wfx.wBitsPerSample > 16)
            return VFW_E_TYPE_NOT_ACCEPTED;
        if (wfx.nSamplesPerSec != 0 &&
            !IsSupportedSampleRate(wfx.nSamplesPerSec))
            return VFW_E_TYPE_NOT_ACCEPTED;
    }

    return S_OK;
}

// ── CheckTransform ────────────────────────────────────────────────────
// Verify that the input → output transform is valid.

HRESULT CMADFilterReloaded::CheckTransform(const CMediaType* pmtIn,
                                            const CMediaType* pmtOut) {
    CheckPointer(pmtIn, E_POINTER);
    CheckPointer(pmtOut, E_POINTER);

    // Output must be PCM audio
    if (*pmtOut->Type()    != MEDIATYPE_Audio)  return VFW_E_TYPE_NOT_ACCEPTED;
    if (*pmtOut->Subtype() != MEDIASUBTYPE_PCM)  return VFW_E_TYPE_NOT_ACCEPTED;

    WAVEFORMATEX wfxOut;
    if (!TryGetWaveFormat(pmtOut, &wfxOut))
        return VFW_E_TYPE_NOT_ACCEPTED;

    if (wfxOut.wFormatTag != WAVE_FORMAT_PCM ||
        wfxOut.wBitsPerSample != 16 ||
        wfxOut.nChannels < 1 || wfxOut.nChannels > 2 ||
        !IsSupportedSampleRate(wfxOut.nSamplesPerSec) ||
        wfxOut.nBlockAlign != wfxOut.nChannels * sizeof(int16_t) ||
        wfxOut.nAvgBytesPerSec !=
            wfxOut.nSamplesPerSec * wfxOut.nBlockAlign)
    {
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    // If the source format is known, the output must match its rate and
    // channel count exactly.  A mono MP3 must never negotiate stereo.
    WAVEFORMATEX wfxIn;
    if (TryGetWaveFormat(pmtIn, &wfxIn)) {
        if (wfxIn.nSamplesPerSec != 0 &&
            wfxIn.nSamplesPerSec != wfxOut.nSamplesPerSec) {
            return VFW_E_TYPE_NOT_ACCEPTED;
        }
        if (wfxIn.nChannels != 0 &&
            wfxIn.nChannels != wfxOut.nChannels) {
            return VFW_E_TYPE_NOT_ACCEPTED;
        }
    }

    return S_OK;
}

// ── GetMediaType ──────────────────────────────────────────────────────
// Offer PCM output types. The exact format is determined after the first
// frame is decoded, but we offer reasonable defaults.

HRESULT CMADFilterReloaded::GetMediaType(int iPos, CMediaType* pmt) {
    CheckPointer(pmt, E_POINTER);

    if (iPos < 0) return E_INVALIDARG;

    auto offer = [&](DWORD rate, WORD channels) -> HRESULT {
        WAVEFORMATEX wfx;
        SetPcmWaveFormat(&wfx, rate, channels);
        pmt->SetType(&MEDIATYPE_Audio);
        pmt->SetSubtype(&MEDIASUBTYPE_PCM);
        pmt->SetFormatType(&FORMAT_WaveFormatEx);
        pmt->SetFormat(reinterpret_cast<BYTE*>(&wfx), sizeof(wfx));
        pmt->SetSampleSize(wfx.nBlockAlign);
        return S_OK;
    };

    // The connected input format is authoritative once known.  Offer
    // exactly one output type so mono sources cannot negotiate stereo.
    if (m_input_format_valid) {
        if (iPos > 0) return VFW_S_NO_MORE_ITEMS;
        return offer(m_input_format.nSamplesPerSec, m_input_format.nChannels);
    }

    // Fallback for sources that do not expose a usable WAVEFORMATEX.
    if (m_output_format_set) {
        if (iPos > 0) return VFW_S_NO_MORE_ITEMS;
        return offer(m_output_format.nSamplesPerSec, m_output_format.nChannels);
    }

    // Full MPEG rate matrix, each stereo then mono.  Ordering only matters
    // for unknown sources; known sources use the exact input format above.
    static const DWORD rates[] = {
        44100, 48000, 32000, 24000, 22050, 16000, 12000, 11025, 8000
    };
    static const WORD chans[] = { 2, 1 };
    constexpr int total_combos =
        static_cast<int>(sizeof(rates) / sizeof(rates[0]) * 2);

    if (iPos >= total_combos)
        return VFW_S_NO_MORE_ITEMS;

    const int rate_idx = iPos / 2;
    const int chan_idx = iPos % 2;
    return offer(rates[rate_idx], chans[chan_idx]);
}

// ── DecideBufferSize ──────────────────────────────────────────────────

HRESULT CMADFilterReloaded::DecideBufferSize(IMemAllocator* pAlloc,
                                              ALLOCATOR_PROPERTIES* pProps) {
    CheckPointer(pAlloc, E_POINTER);
    CheckPointer(pProps, E_POINTER);

    // Get input pin to determine input buffer size
    CBaseInputPin* pIn = m_pInput;
    if (!pIn) return E_UNEXPECTED;

    // The output allocator must be able to hold at least one complete
    // decoded frame.  Larger downstream proposals are preserved because
    // they reduce the number of partial samples; smaller ones are raised
    // to the one-frame floor.
    LONG outputSize =
        pProps->cbBuffer > 0 ? pProps->cbBuffer : DEFAULT_INPUT_SIZE;
    if (outputSize < MIN_OUTPUT_SIZE) {
        outputSize = MIN_OUTPUT_SIZE;
    }
    if (outputSize < OUTPUT_BUFFER_FLOOR) {
        outputSize = OUTPUT_BUFFER_FLOOR;
    }
    if (outputSize > MAX_OUTPUT_SIZE) {
        outputSize = MAX_OUTPUT_SIZE;
    }

    pProps->cBuffers = 1;           // One output buffer (transform filter)
    pProps->cbBuffer = outputSize;
    pProps->cbAlign  = 1;
    pProps->cbPrefix = 0;

    // Get allocator requirements
    ALLOCATOR_PROPERTIES actual;
    HRESULT hr = pAlloc->SetProperties(pProps, &actual);
    if (FAILED(hr)) return hr;

    // A smaller-than-one-frame output buffer cannot make progress without
    // dropping PCM, so reject it explicitly.
    if (actual.cbBuffer < MIN_OUTPUT_SIZE) {
        return E_FAIL;
    }

    return S_OK;
}

// ── Transform ─────────────────────────────────────────────────────────
// The core: decode one input sample into one output sample.

HRESULT CMADFilterReloaded::Transform(IMediaSample* pIn, IMediaSample* pOut) {
    CheckPointer(pIn, E_POINTER);
    CheckPointer(pOut, E_POINTER);

    // ── Get input data ────────────────────────────────────────────────
    UpdateSourcePolicy();

    BYTE* pInData = nullptr;
    HRESULT hr = pIn->GetPointer(&pInData);
    if (FAILED(hr)) return hr;

    LONG inLen = pIn->GetActualDataLength();
    if (pInData && inLen > 0 && m_first_input_len < sizeof(m_first_input)) {
        size_t copy = sizeof(m_first_input) - m_first_input_len;
        if (copy > static_cast<size_t>(inLen)) copy = static_cast<size_t>(inLen);
        std::memcpy(m_first_input + m_first_input_len, pInData, copy);
        m_first_input_len += copy;
    }
    REFERENCE_TIME input_start = 0;
    REFERENCE_TIME input_end = 0;
    const HRESULT input_time_result =
        pIn->GetTime(&input_start, &input_end);
    const bool have_input_time = SUCCEEDED(input_time_result);
    const bool input_discontinuity = pIn->IsDiscontinuity() == S_OK;

    if (m_segment_pending) {
        bool continuation = false;
        if (m_bsaudio_mode && !m_force_reset &&
            m_stream_time_base_valid &&
            m_have_last_segment_start) {
            const REFERENCE_TIME delta =
                m_segment_start - m_last_segment_start;
            continuation = delta >= 0 && delta <= MAX_BSAUDIO_CONTINUATION;
        }
        m_force_reset = false;

        if (have_input_time) {
            m_diag.record(
                madfilter::DIAG_INPUT_TIMESTAMP,
                static_cast<uint32_t>(input_start / 10000),
                static_cast<uint32_t>(m_segment_start / 10000));
        }

        if (continuation) {
            // Fallout 3 radio delivers one continuous stream in many short
            // segments.  Preserve the decoder, reservoir, and timestamp
            // base; this is not a seek, so do not flag every sample as a
            // discontinuity.
            m_diag.record(madfilter::DIAG_CONTINUATION);
            m_discontinuity_pending = false;
        } else {
            // Initial segment, real seek, or station change.
            m_diag.record(madfilter::DIAG_SEGMENT_RESET);
            madfilter::maddec_reset(&m_decoder);
            ClearInputCarry();
            ResetDiagnosticBaseline();
            m_format_mismatch_reported = false;
            // NewSegment carries an absolute media position; sample times
            // restart relative to this segment, including optional preroll.
            m_stream_time_base = have_input_time ? input_start : 0;
            m_stream_time_base_valid = true;
            m_discontinuity_pending = true;
        }

        m_last_segment_start = m_segment_start;
        m_have_last_segment_start = true;
        m_segment_pending = false;
        // Segment logging is deferred to Stop/destructor: radio can call
        // NewSegment every few milliseconds.

    } else if (input_discontinuity) {
        m_diag.record(madfilter::DIAG_INPUT_DISCONTINUITY);
        // BSAudio sets this on micro-chunks too; NewSegment owns its
        // reset decisions.  For ordinary sources, honor a discontinuity
        // that arrives without a segment.
        if (!m_bsaudio_mode) {
            if (m_stream_time_base_valid) {
                m_stream_time_base +=
                    mad_timer_to_reference_time(m_decoder.timer);
            }
            madfilter::maddec_reset(&m_decoder);
            ClearInputCarry();
            ResetDiagnosticBaseline();
            if (have_input_time && input_start >= 0) {
                m_stream_time_base = input_start;
                m_stream_time_base_valid = true;
            }
            m_discontinuity_pending = true;
        }
    } else if (have_input_time && input_start >= 0 &&
               !m_stream_time_base_valid) {
        m_stream_time_base =
            input_start - mad_timer_to_reference_time(m_decoder.timer);
        m_stream_time_base_valid = true;
    }

    if (inLen <= 0 || !pInData) {
        // Empty input — output nothing, but clear any stale sample state.
        pOut->SetActualDataLength(0);
        pOut->SetTime(nullptr, nullptr);
        pOut->SetMediaTime(nullptr, nullptr);
        pOut->SetDiscontinuity(FALSE);
        pOut->SetSyncPoint(FALSE);
        pOut->SetPreroll(FALSE);
        return S_OK;
    }

    // ── Get output buffer ─────────────────────────────────────────────
    BYTE* pOutData = nullptr;
    hr = pOut->GetPointer(&pOutData);
    if (FAILED(hr)) return hr;

    LONG outCapacity = pOut->GetSize();
    if (outCapacity <= 0) {
        return VFW_E_BUFFER_OVERFLOW;
    }

    const size_t input_size = static_cast<size_t>(inLen);
    const size_t output_capacity = static_cast<size_t>(outCapacity);
    if (output_capacity < static_cast<size_t>(MIN_OUTPUT_SIZE)) {
        // A smaller buffer cannot hold one decoded frame, so there is no
        // way to make progress without dropping PCM.
        return VFW_E_BUFFER_OVERFLOW;
    }

    // ── Decode, retaining any compressed input that does not fit ─────
    size_t written = 0;
    madfilter::SyncState syncState = m_decoder.sync_state;
    const uint64_t start_frames = m_decoder.total_frames_decoded;
    auto frames_left = [&]() -> size_t {
        const uint64_t decoded = m_decoder.total_frames_decoded - start_frames;
        return decoded >= MAX_DECODE_FRAMES_PER_TRANSFORM ? 0 :
            static_cast<size_t>(MAX_DECODE_FRAMES_PER_TRANSFORM - decoded);
    };

    // Drain bytes carried from an earlier sample first.
    if (m_input_carry_len > 0) {
        hr = DrainInputCarry(
            pOutData, output_capacity, &written, &syncState, frames_left());
        if (FAILED(hr)) return hr;
    }

    bool input_accounted = false;
    if (m_input_carry_len == 0 && written < output_capacity) {
        size_t consumed = 0;
        size_t produced = 0;
        hr = DecodeInput(
            reinterpret_cast<const uint8_t*>(pInData), input_size,
            pOutData + written, output_capacity - written,
            &consumed, &produced, &syncState, frames_left());
        if (FAILED(hr)) return hr;

        written += produced;

        if (consumed < input_size) {
            const size_t remaining = input_size - consumed;
            if (remaining > INPUT_CARRY_CAPACITY) {
                return VFW_E_BUFFER_OVERFLOW;
            }
            std::memcpy(
                m_input_carry, pInData + consumed, remaining);
            m_input_carry_len = remaining;
        }
        input_accounted = true;
    }

    if (!input_accounted) {
        // Output is full or earlier carry is still pending: retain this
        // sample's compressed bytes instead of failing the stream.
        if (input_size > INPUT_CARRY_CAPACITY - m_input_carry_len) {
            return VFW_E_BUFFER_OVERFLOW;
        }
        std::memcpy(
            m_input_carry + m_input_carry_len, pInData, input_size);
        m_input_carry_len += input_size;

        if (written < output_capacity) {
            size_t produced = 0;
            hr = DrainInputCarry(
                pOutData + written, output_capacity - written,
                &produced, &syncState, frames_left());
            if (FAILED(hr)) return hr;
            written += produced;
        }
    }

    UpdateDiagnostics(m_decoder.sync_state);
    if (written == 0 && input_size > 0 &&
        m_decoder.sync_state == madfilter::SyncState::SYNCHRONIZED) {
        m_diag.record(
            madfilter::DIAG_UNDERFLOW,
            static_cast<uint32_t>(input_size));
    }

    hr = ValidateOutputFormat();
    if (FAILED(hr)) return hr;

    // ── Set output sample properties ──────────────────────────────────
    pOut->SetActualDataLength(static_cast<LONG>(written));

    if (written > 0) {
        hr = SetOutputTimestamp(pOut, written);
        if (FAILED(hr)) return hr;

        m_total_pcm_bytes += written;
        REFERENCE_TIME out_start = 0;
        REFERENCE_TIME out_end = 0;
        if (pOut->GetTime(&out_start, &out_end) == S_OK) {
            if (!m_have_output_time) {
                m_first_output_time = out_start;
                m_have_output_time = true;
            }
            m_last_output_time = out_end;
        }

        pOut->SetDiscontinuity(
            m_discontinuity_pending ? TRUE : FALSE);
        m_discontinuity_pending = false;
    } else {
        pOut->SetTime(nullptr, nullptr);
        pOut->SetDiscontinuity(FALSE);
    }

    pOut->SetMediaTime(nullptr, nullptr);
    pOut->SetSyncPoint(
        pIn->IsSyncPoint() == S_OK ? TRUE : FALSE);
    pOut->SetPreroll(
        pIn->IsPreroll() == S_OK ? TRUE : FALSE);

    return S_OK;
}

HRESULT CMADFilterReloaded::ValidateOutputFormat() {
    if (!m_output_format_set &&
        m_decoder.sync_state == madfilter::SyncState::SYNCHRONIZED) {
        SetPcmWaveFormat(
            &m_output_format,
            m_decoder.sample_rate,
            static_cast<WORD>(m_decoder.channels));
        m_output_format_set = true;
    } else if (m_output_format_set &&
               m_decoder.sample_rate != 0 &&
               m_decoder.channels != 0 &&
               (m_output_format.nSamplesPerSec != m_decoder.sample_rate ||
                m_output_format.nChannels != m_decoder.channels)) {
        m_diag.record(
            madfilter::DIAG_FORMAT_MISMATCH,
            m_decoder.sample_rate,
            m_decoder.channels);

        // The negotiated output type is fixed for this connection.  Never
        // silently label PCM with the wrong rate or channel count.
        if (!m_format_mismatch_reported) {
            OutputDebugStringA(
                "MADFilter Reloaded: decoded rate/channels differ from "
                "negotiated output\n");
            m_format_mismatch_reported = true;
        }
        return VFW_E_TYPE_NOT_ACCEPTED;
    }
    return S_OK;
}

// ── Stop ──────────────────────────────────────────────────────────────

STDMETHODIMP CMADFilterReloaded::Stop() {
    const HRESULT hr = CTransformFilter::Stop();
    m_force_reset = true;
    DumpDiagnostics("stop");
    return hr;
}

// ── BeginFlush ────────────────────────────────────────────────────────

HRESULT CMADFilterReloaded::BeginFlush() {
    m_diag.record(madfilter::DIAG_FLUSH_BEGIN);
    // Do not reset the decoder here.  BeginFlush is not synchronized with
    // Receive, and Fallout 3 flushes between contiguous radio chunks.  The
    // decision to reset is made in NewSegment after comparing timestamps.
    m_discontinuity_pending = true;
    if (m_pOutput && m_pOutput->IsConnected()) {
        return CTransformFilter::BeginFlush();
    }
    return S_OK;
}

// ── EndFlush ──────────────────────────────────────────────────────────

HRESULT CMADFilterReloaded::EndFlush() {
    m_diag.record(madfilter::DIAG_FLUSH_END);
    // Decoder reset (if needed) is deferred to NewSegment.
    if (m_pOutput && m_pOutput->IsConnected()) {
        return CTransformFilter::EndFlush();
    }
    return S_OK;
}

// ── NewSegment ────────────────────────────────────────────────────────

HRESULT CMADFilterReloaded::NewSegment(REFERENCE_TIME tStart,
                                        REFERENCE_TIME tStop, double dRate) {
    m_diag.record(
        madfilter::DIAG_SEGMENT,
        static_cast<uint32_t>(tStart),
        static_cast<uint32_t>(tStart >> 32));

    // Decide continuation vs reset on the first sample so a burst of
    // micro-segments does not repeatedly reset the decoder.
    m_segment_start = tStart;
    m_segment_pending = true;
    m_discontinuity_pending = true;

    if (m_pOutput && m_pOutput->IsConnected()) {
        return CTransformFilter::NewSegment(tStart, tStop, dRate);
    }
    return S_OK;
}

// ── EndOfStream ───────────────────────────────────────────────────────

HRESULT CMADFilterReloaded::EndOfStream() {
    if (m_pOutput && m_pOutput->IsConnected()) {
        const uint8_t empty = 0;

        while (m_input_carry_len > 0 || m_decoder.ibuf_len > 0) {
            IMediaSample* output_sample = nullptr;
            HRESULT hr = m_pOutput->GetDeliveryBuffer(
                &output_sample, nullptr, nullptr, 0);
            if (FAILED(hr)) {
                return hr;
            }

            BYTE* output_data = nullptr;
            hr = output_sample->GetPointer(&output_data);
            if (FAILED(hr)) {
                output_sample->Release();
                return hr;
            }

            const LONG output_capacity = output_sample->GetSize();
            if (!output_data ||
                output_capacity < MIN_OUTPUT_SIZE) {
                output_sample->Release();
                return VFW_E_BUFFER_OVERFLOW;
            }

            size_t written = 0;
            madfilter::SyncState sync_state = m_decoder.sync_state;

            if (m_input_carry_len > 0) {
                hr = DrainInputCarry(
                    output_data,
                    static_cast<size_t>(output_capacity),
                    &written,
                    &sync_state, MAX_DECODE_FRAMES_PER_TRANSFORM);
                if (FAILED(hr)) {
                    output_sample->Release();
                    return hr;
                }
            }

            // Once the compressed carry is empty, expose libmad's zeroed
            // guard bytes so the final complete frame can be emitted.
            if (written == 0 && m_input_carry_len == 0 &&
                m_decoder.ibuf_len > 0) {
                size_t consumed = 0;
                const int result = madfilter::maddec_decode(
                    &m_decoder,
                    &empty,
                    0,
                    reinterpret_cast<int16_t*>(output_data),
                    static_cast<size_t>(output_capacity),
                    &consumed,
                    &written,
                    &sync_state);

                if (result == -1 || consumed != 0) {
                    output_sample->Release();
                    return E_FAIL;
                }
            }

            UpdateDiagnostics(m_decoder.sync_state);

            if (written == 0) {
                output_sample->Release();
                break;
            }

            hr = output_sample->SetActualDataLength(
                static_cast<LONG>(written));
            if (SUCCEEDED(hr)) {
                hr = SetOutputTimestamp(output_sample, written);
                if (SUCCEEDED(hr)) {
                    m_total_pcm_bytes += written;
                }
            }
            if (SUCCEEDED(hr)) {
                output_sample->SetMediaTime(nullptr, nullptr);
                output_sample->SetSyncPoint(TRUE);
                output_sample->SetPreroll(FALSE);
                output_sample->SetDiscontinuity(
                    m_discontinuity_pending ? TRUE : FALSE);
                m_discontinuity_pending = false;
                hr = m_pOutput->Deliver(output_sample);
            }

            output_sample->Release();
            if (FAILED(hr)) {
                return hr;
            }
        }
    }

    DumpDiagnostics("end-of-stream");
    return CTransformFilter::EndOfStream();
}
