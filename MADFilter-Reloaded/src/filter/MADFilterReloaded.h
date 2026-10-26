// MADFilterReloaded.h — DirectShow MPEG audio transform filter
//
// 32-bit x86 DirectShow transform filter using Tenacity libmad.
// Rebuild of the original MADFilter concept by Armin Gerritsen (2002),
// with instance-based decoder state, conservative merit, and bounded
// buffering.

#pragma once

#include <streams.h>          // DirectShow base classes
#include "CLSIDs.h"
#include "diagnostics.h"
#include "maddecode.h"

// ── Filter class ──────────────────────────────────────────────────────

class CMADFilterReloaded : public CTransformFilter
{
public:
    // ── COM factory ───────────────────────────────────────────────────
    static CUnknown* WINAPI CreateInstance(LPUNKNOWN punk, HRESULT* phr);

    // ── IUnknown ──────────────────────────────────────────────────────
    STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv);

    // ── Diagnostics (test and debugger access) ────────────────────────
    size_t DiagnosticCount() const { return m_diag.size(); }
    const madfilter::DiagEvent& DiagnosticAt(size_t index) const {
        return m_diag.at(index);
    }

    // ── CTransformFilter overrides ────────────────────────────────────
    HRESULT CheckInputType(const CMediaType* pmtIn) override;
    HRESULT CheckTransform(const CMediaType* pmtIn,
                           const CMediaType* pmtOut) override;
    HRESULT GetMediaType(int iPos, CMediaType* pmt) override;
    HRESULT SetMediaType(PIN_DIRECTION direction,
                         const CMediaType* pmt) override;
    HRESULT DecideBufferSize(IMemAllocator* pAlloc,
                             ALLOCATOR_PROPERTIES* pProps) override;
    HRESULT Transform(IMediaSample* pIn, IMediaSample* pOut) override;
    HRESULT Receive(IMediaSample* pIn) override;

    // ── Streaming event overrides ─────────────────────────────────────
    STDMETHODIMP Stop() override;
    HRESULT BeginFlush() override;
    HRESULT EndFlush() override;
    HRESULT NewSegment(REFERENCE_TIME tStart, REFERENCE_TIME tStop,
                       double dRate) override;
    HRESULT EndOfStream() override;

private:
    // Construction only through CreateInstance
    CMADFilterReloaded(LPUNKNOWN punk, HRESULT* phr);
    ~CMADFilterReloaded();

    // Prevent copy
    CMADFilterReloaded(const CMADFilterReloaded&) = delete;
    CMADFilterReloaded& operator=(const CMADFilterReloaded&) = delete;

    HRESULT SetOutputTimestamp(
        IMediaSample* sample, size_t pcm_bytes);
    HRESULT ValidateOutputFormat();

    HRESULT DecodeInput(const uint8_t* data, size_t length,
                        BYTE* out, size_t out_capacity,
                        size_t* consumed, size_t* written,
                        madfilter::SyncState* sync_state,
                        size_t max_frames);

    HRESULT DrainInputCarry(BYTE* out, size_t out_capacity,
                            size_t* written,
                            madfilter::SyncState* sync_state,
                            size_t max_frames);

    void ClearInputCarry();
    void ResetDiagnosticBaseline();
    void UpdateDiagnostics(madfilter::SyncState sync_state);
    void DumpDiagnostics(const char* reason) const;
    void UpdateSourcePolicy();
    HRESULT DecodeExtraOutput(BYTE* out, size_t out_capacity,
                              size_t* written,
                              madfilter::SyncState* sync_state,
                              size_t max_frames);

    // ── Decoder state ─────────────────────────────────────────────────
    madfilter::MadDecoder  m_decoder;
    madfilter::DiagRing    m_diag;
    madfilter::SyncState   m_last_sync_state;
    uint64_t               m_last_decode_errors;
    uint64_t               m_last_concealed_frames;
    uint64_t               m_last_discarded_frames;
    uint32_t               m_last_sample_rate;
    uint8_t                m_last_channels;
    bool                   m_last_scan_exhausted;
    size_t                 m_last_carry_len;
    uint64_t               m_total_pcm_bytes;
    uint64_t               m_received_samples;
    uint64_t               m_received_bytes;
    uint64_t               m_nonmedia_samples;
    uint8_t                m_first_input[16];
    size_t                 m_first_input_len;
    REFERENCE_TIME         m_first_output_time;
    REFERENCE_TIME         m_last_output_time;
    bool                   m_have_output_time;
    bool                   m_force_reset;
    bool                   m_segment_pending;
    bool                   m_have_last_segment_start;
    REFERENCE_TIME         m_segment_start;
    REFERENCE_TIME         m_last_segment_start;
    bool                   m_output_format_set;
    bool                   m_emit_timestamps;
    bool                   m_bsaudio_mode;
    bool                   m_source_checked;
    bool                   m_input_format_valid;
    bool                   m_format_mismatch_reported;
    bool                   m_discontinuity_pending;
    REFERENCE_TIME         m_stream_time_base;
    bool                   m_stream_time_base_valid;

    // ── Bounded compressed-input carry (no drops in Transform) ───────
    static constexpr size_t MAX_DECODE_FRAMES_PER_TRANSFORM = 64;
    static constexpr size_t MAX_EXTRA_OUTPUTS_PER_RECEIVE = 32;
    static constexpr uint64_t MAX_EXTRA_DECODE_FRAMES = 64;
    static constexpr size_t INPUT_CARRY_CAPACITY = 256 * 1024;
    uint8_t                m_input_carry[INPUT_CARRY_CAPACITY];
    size_t                 m_input_carry_len;

    // ── Stream format ─────────────────────────────────────────────────
    WAVEFORMATEX           m_input_format;
    WAVEFORMATEX           m_output_format;
};
