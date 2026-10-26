#include "MADFilterReloaded.h"

#include <cstdint>
#include <cstdio>
#include <new>
#include <vector>

namespace {

constexpr size_t kFrameBytes = 417;
constexpr size_t kFrameCount = 30;
constexpr size_t kDiscardedPrerollFrames = 4;
constexpr size_t kBufferedTailFrames = 1;
constexpr size_t kPcmBytesPerFrame = 1152 * 2 * sizeof(int16_t);
constexpr REFERENCE_TIME kReferenceTimePerSecond = 10000000;

REFERENCE_TIME frame_time(size_t frame_count) {
    const LONGLONG samples =
        static_cast<LONGLONG>(frame_count) * 1152;
    return static_cast<REFERENCE_TIME>(
        (samples * kReferenceTimePerSecond + 44100 / 2) / 44100);
}

template<typename T>
class FilterOwner {
public:
    explicit FilterOwner(T* filter)
        : m_filter(filter)
    {
        if (m_filter) {
            m_filter->NonDelegatingAddRef();
        }
    }

    ~FilterOwner() {
        if (m_filter) {
            m_filter->NonDelegatingRelease();
        }
    }

    T* operator->() const {
        return m_filter;
    }

private:
    T* m_filter;
};

const CLSID kTestSinkClsid = {
    0x8c7c78ba, 0xf117, 0x4a3d,
    {0x93, 0x98, 0xb7, 0x61, 0x82, 0xf3, 0x38, 0x13}
};

const CLSID kTestSourceClsid = {
    0xe3a96ca0, 0x53bb, 0x4dc3,
    {0xa8, 0xb8, 0x31, 0xe5, 0xce, 0xdb, 0xf4, 0xef}
};

CMediaType make_mpeg_audio_type(WORD channels = 2, DWORD rate = 44100) {
    WAVEFORMATEX format = {};
    format.wFormatTag = 0x0050; // WAVE_FORMAT_MPEG
    format.nChannels = channels;
    format.nSamplesPerSec = rate;

    CMediaType media_type;
    media_type.SetType(&MEDIATYPE_Audio);
    media_type.SetSubtype(&MEDIASUBTYPE_MPEG1Audio);
    media_type.SetFormatType(&FORMAT_WaveFormatEx);
    media_type.SetFormat(
        reinterpret_cast<BYTE*>(&format), sizeof(format));
    media_type.SetSampleSize(1);
    return media_type;
}

CMediaType make_pcm_type(WORD channels, DWORD rate, WORD bits = 16) {
    WAVEFORMATEX format = {};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = channels;
    format.nSamplesPerSec = rate;
    format.wBitsPerSample = bits;
    format.nBlockAlign = static_cast<WORD>(channels * bits / 8);
    format.nAvgBytesPerSec = rate * format.nBlockAlign;

    CMediaType media_type;
    media_type.SetType(&MEDIATYPE_Audio);
    media_type.SetSubtype(&MEDIASUBTYPE_PCM);
    media_type.SetFormatType(&FORMAT_WaveFormatEx);
    media_type.SetFormat(
        reinterpret_cast<BYTE*>(&format), sizeof(format));
    media_type.SetSampleSize(format.nBlockAlign);
    return media_type;
}

bool has_diag(
    CMADFilterReloaded* filter, uint32_t code,
    uint32_t expected_a = UINT32_MAX,
    uint32_t expected_b = UINT32_MAX)
{
    for (size_t i = 0; i < filter->DiagnosticCount(); ++i) {
        const madfilter::DiagEvent& event = filter->DiagnosticAt(i);
        if (event.code != code) continue;
        if (expected_a != UINT32_MAX && event.a != expected_a) continue;
        if (expected_b != UINT32_MAX && event.b != expected_b) continue;
        return true;
    }
    return false;
}

int run_format_tests(CMADFilterReloaded* filter) {
    int failures = 0;

    auto expect = [&](bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "%s\n", message);
            ++failures;
        }
    };

    CMediaType mono22 = make_mpeg_audio_type(1, 22050);
    HRESULT hr = filter->SetMediaType(PINDIR_INPUT, &mono22);
    expect(SUCCEEDED(hr), "failed to set 22.05 kHz mono input type");
    expect(
        has_diag(filter, madfilter::DIAG_INPUT_FORMAT, 22050, 1),
        "diagnostics did not record the input format");

    CMediaType output;
    hr = filter->GetMediaType(0, &output);
    expect(hr == S_OK, "GetMediaType failed for 22.05 kHz mono");
    if (hr == S_OK) {
        WAVEFORMATEX* wfx =
            reinterpret_cast<WAVEFORMATEX*>(output.Format());
        expect(wfx != nullptr, "GetMediaType returned no WAVEFORMATEX");
        if (wfx) {
            expect(wfx->nSamplesPerSec == 22050,
                   "22.05 kHz output rate was not preserved");
            expect(wfx->nChannels == 1,
                   "mono source negotiated non-mono output");
            expect(wfx->wBitsPerSample == 16,
                   "output was not 16-bit PCM");
            expect(wfx->nBlockAlign == 2,
                   "mono output block alignment was wrong");
        }
    }

    CMediaType extra;
    hr = filter->GetMediaType(1, &extra);
    expect(hr == VFW_S_NO_MORE_ITEMS,
           "GetMediaType offered more than the exact input format");

    CMediaType good = make_pcm_type(1, 22050);
    expect(filter->CheckTransform(&mono22, &good) == S_OK,
           "mono 22.05 kHz transform was rejected");

    CMediaType wrong_rate = make_pcm_type(1, 44100);
    expect(filter->CheckTransform(&mono22, &wrong_rate) ==
               VFW_E_TYPE_NOT_ACCEPTED,
           "rate mismatch was accepted");

    CMediaType wrong_channels = make_pcm_type(2, 22050);
    expect(filter->CheckTransform(&mono22, &wrong_channels) ==
               VFW_E_TYPE_NOT_ACCEPTED,
           "mono-to-stereo transform was accepted");

    CMediaType wrong_bits = make_pcm_type(1, 22050, 8);
    expect(filter->CheckTransform(&mono22, &wrong_bits) ==
               VFW_E_TYPE_NOT_ACCEPTED,
           "non-16-bit output was accepted");

    CMediaType bad_input = make_mpeg_audio_type(3, 44100);
    expect(filter->CheckInputType(&bad_input) ==
               VFW_E_TYPE_NOT_ACCEPTED,
           "3-channel input was accepted");

    CMediaType bad_rate = make_mpeg_audio_type(1, 88200);
    expect(filter->CheckInputType(&bad_rate) ==
               VFW_E_TYPE_NOT_ACCEPTED,
           "unsupported input rate was accepted");

    CMediaType stereo44 = make_mpeg_audio_type(2, 44100);
    hr = filter->SetMediaType(PINDIR_INPUT, &stereo44);
    expect(SUCCEEDED(hr), "failed to restore stereo 44.1 kHz input type");

    return failures;
}

class TestSource;

class TestOutputPin final : public CBaseOutputPin {
public:
    TestOutputPin(TestSource* source, CCritSec* lock, HRESULT* phr);

    HRESULT CheckMediaType(const CMediaType* media_type) override {
        if (media_type &&
            *media_type->Type() == MEDIATYPE_Audio &&
            *media_type->Subtype() == MEDIASUBTYPE_MPEG1Audio) {
            return S_OK;
        }
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    HRESULT GetMediaType(int position, CMediaType* media_type) override {
        if (position < 0) {
            return E_INVALIDARG;
        }
        if (position > 0) {
            return VFW_S_NO_MORE_ITEMS;
        }
        *media_type = m_media_type;
        return S_OK;
    }

    HRESULT DecideBufferSize(
        IMemAllocator* allocator,
        ALLOCATOR_PROPERTIES* properties) override
    {
        properties->cBuffers = 1;
        properties->cbBuffer = 8192;
        properties->cbAlign = 1;
        properties->cbPrefix = 0;

        ALLOCATOR_PROPERTIES actual = {};
        HRESULT hr = allocator->SetProperties(properties, &actual);
        if (FAILED(hr)) {
            return hr;
        }
        return actual.cbBuffer >= properties->cbBuffer ? S_OK : E_FAIL;
    }

    const CMediaType& MediaType() const {
        return m_media_type;
    }

private:
    CMediaType m_media_type;
};

class TestSource final : public CBaseFilter {
public:
    explicit TestSource(HRESULT* phr)
        : CBaseFilter(
              NAME("filter_sample_test source"), nullptr, &m_lock,
              kTestSourceClsid)
        , m_pin(this, &m_lock, phr)
    {
    }

    int GetPinCount() override {
        return 1;
    }

    CBasePin* GetPin(int index) override {
        return index == 0 ? &m_pin : nullptr;
    }

    TestOutputPin* OutputPin() {
        return &m_pin;
    }

private:
    CCritSec m_lock;
    TestOutputPin m_pin;
};

TestOutputPin::TestOutputPin(
    TestSource* source, CCritSec* lock, HRESULT* phr)
    : CBaseOutputPin(
          NAME("filter_sample_test output pin"), source, lock, phr, L"Output")
    , m_media_type(make_mpeg_audio_type())
{
}

class CapturingSink;

class CapturingInputPin final : public CBaseInputPin {
public:
    CapturingInputPin(CapturingSink* sink, CCritSec* lock, HRESULT* phr);

    HRESULT CheckMediaType(const CMediaType* media_type) override {
        if (media_type &&
            *media_type->Type() == MEDIATYPE_Audio &&
            *media_type->Subtype() == MEDIASUBTYPE_PCM) {
            return S_OK;
        }
        return VFW_E_TYPE_NOT_ACCEPTED;
    }

    STDMETHODIMP Receive(IMediaSample* sample) override;
    STDMETHODIMP EndOfStream() override;

private:
    CapturingSink* m_sink;
};

class CapturingSink final : public CBaseFilter {
public:
    explicit CapturingSink(HRESULT* phr)
        : CBaseFilter(
              NAME("filter_sample_test sink"), nullptr, &m_lock,
              kTestSinkClsid)
        , m_pin(this, &m_lock, phr)
        , sample_count(0)
        , pcm_bytes(0)
        , discontinuous(false)
        , eos_received(false)
        , time_valid(false)
        , time_start(0)
        , time_end(0)
    {
    }

    int GetPinCount() override {
        return 1;
    }

    CBasePin* GetPin(int index) override {
        return index == 0 ? &m_pin : nullptr;
    }

    void Capture(IMediaSample* sample) {
        ++sample_count;
        pcm_bytes += sample->GetActualDataLength();
        discontinuous = sample->IsDiscontinuity() == S_OK;
        time_valid = sample->GetTime(&time_start, &time_end) == S_OK;
    }

private:
    CCritSec m_lock;
    CapturingInputPin m_pin;

public:
    int sample_count;
    LONG pcm_bytes;
    bool discontinuous;
    bool eos_received;
    bool time_valid;
    REFERENCE_TIME time_start;
    REFERENCE_TIME time_end;
};

CapturingInputPin::CapturingInputPin(
    CapturingSink* sink, CCritSec* lock, HRESULT* phr)
    : CBaseInputPin(
          NAME("filter_sample_test input pin"), sink, lock, phr, L"Input")
    , m_sink(sink)
{
}

STDMETHODIMP CapturingInputPin::Receive(IMediaSample* sample) {
    HRESULT hr = CBaseInputPin::Receive(sample);
    if (SUCCEEDED(hr)) {
        m_sink->Capture(sample);
    }
    return hr;
}

STDMETHODIMP CapturingInputPin::EndOfStream() {
    m_sink->eos_received = true;
    return S_OK;
}

std::vector<uint8_t> make_silent_layer2_frames(
    size_t frame_count, WORD channels = 2) {
    std::vector<uint8_t> data(frame_count * kFrameBytes, 0);
    for (size_t i = 0; i < frame_count; ++i) {
        uint8_t* frame = data.data() + i * kFrameBytes;
        frame[0] = 0xFF; // MPEG-1, Layer II, 128 kbps, 44.1 kHz
        frame[1] = 0xFD;
        frame[2] = 0x80;
        frame[3] = channels == 1 ? 0xC0 : 0x00;
    }
    return data;
}

int run_buffer_size_test(CMADFilterReloaded* filter) {
    // Pins are created lazily by CTransformFilter::GetPin.
    filter->GetPin(0);

    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("buffer size allocator"), nullptr, &hr);
    if (FAILED(hr)) {
        std::fprintf(stderr, "buffer size allocator construction failed\n");
        return 1;
    }

    ALLOCATOR_PROPERTIES props = {};
    props.cBuffers = 1;
    props.cbBuffer = 4608;
    props.cbAlign = 1;
    props.cbPrefix = 0;

    hr = filter->DecideBufferSize(&allocator, &props);
    if (hr != S_OK || props.cbBuffer < 16 * 1024) {
        std::fprintf(
            stderr,
            "output buffer floor not requested: hr=0x%08lX size=%ld\n",
            hr, props.cbBuffer);
        return 1;
    }
    return 0;
}

int run_carry_test(CMADFilterReloaded* filter) {
    constexpr size_t kOutputBytes = 4608;

    std::vector<uint8_t> input = make_silent_layer2_frames(kFrameCount);
    std::vector<uint8_t> output(kOutputBytes);

    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("carry test allocator"), nullptr, &hr);
    if (FAILED(hr)) {
        std::fprintf(stderr, "carry allocator construction failed: 0x%08lX\n", hr);
        return 1;
    }

    CMediaSample input_sample(
        NAME("carry test input"), &allocator, &hr,
        input.data(), static_cast<LONG>(input.size()));
    if (FAILED(hr)) {
        std::fprintf(stderr, "carry input construction failed: 0x%08lX\n", hr);
        return 1;
    }

    CMediaSample output_sample(
        NAME("carry test output"), &allocator, &hr,
        output.data(), static_cast<LONG>(output.size()));
    if (FAILED(hr)) {
        std::fprintf(stderr, "carry output construction failed: 0x%08lX\n", hr);
        return 1;
    }

    REFERENCE_TIME start = 0;
    REFERENCE_TIME end = 10000000;
    input_sample.SetTime(&start, &end);

    hr = filter->Transform(&input_sample, &output_sample);
    if (hr != S_OK || output_sample.GetActualDataLength() !=
                          static_cast<LONG>(kOutputBytes)) {
        std::fprintf(
            stderr,
            "large sample with one-frame output failed: hr=0x%08lX len=%ld\n",
            hr, output_sample.GetActualDataLength());
        return 1;
    }
    if (!has_diag(filter, madfilter::DIAG_SYNC_ACQUIRED)) {
        std::fprintf(stderr, "diagnostics did not record sync acquisition\n");
        return 1;
    }

    // Feed one more byte.  The previous sample's unconsumed compressed
    // bytes must still be carried, not dropped or failed.
    uint8_t extra = 0;
    input_sample.SetPointer(&extra, 1);
    input_sample.SetActualDataLength(1);
    output_sample.SetActualDataLength(0);
    hr = filter->Transform(&input_sample, &output_sample);
    if (hr != S_OK) {
        std::fprintf(
            stderr, "carried input sample failed: hr=0x%08lX\n", hr);
        return 1;
    }

    return 0;
}

int run_lifecycle_test(CMADFilterReloaded* filter) {
    auto acceptable = [](HRESULT hr) {
        return SUCCEEDED(hr) || hr == VFW_E_NOT_CONNECTED;
    };

    // Idempotent flush and segment calls on an unconnected filter.
    if (!acceptable(filter->BeginFlush())) return 1;
    if (!acceptable(filter->BeginFlush())) return 1;
    if (!acceptable(filter->EndFlush())) return 1;
    if (!acceptable(filter->EndFlush())) return 1;
    if (!has_diag(filter, madfilter::DIAG_FLUSH_BEGIN) ||
        !has_diag(filter, madfilter::DIAG_FLUSH_END)) {
        std::fprintf(stderr, "diagnostics did not record flush events\n");
        return 1;
    }

    const REFERENCE_TIME segment_start = 5 * kReferenceTimePerSecond;
    if (!acceptable(filter->NewSegment(
            segment_start, segment_start + kReferenceTimePerSecond, 1.0))) {
        return 1;
    }
    if (!acceptable(filter->NewSegment(
            segment_start, segment_start + kReferenceTimePerSecond, 1.0))) {
        return 1;
    }

    std::vector<uint8_t> input = make_silent_layer2_frames(kFrameCount);
    std::vector<uint8_t> output(kPcmBytesPerFrame);

    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("lifecycle allocator"), nullptr, &hr);
    if (FAILED(hr)) {
        std::fprintf(stderr, "lifecycle allocator construction failed\n");
        return 1;
    }

    CMediaSample input_sample(
        NAME("lifecycle input"), &allocator, &hr,
        input.data(), static_cast<LONG>(input.size()));
    CMediaSample output_sample(
        NAME("lifecycle output"), &allocator, &hr,
        output.data(), static_cast<LONG>(output.size()));

    hr = filter->Transform(&input_sample, &output_sample);
    REFERENCE_TIME output_start = 0;
    REFERENCE_TIME output_end = 0;
    if (hr != S_OK ||
        output_sample.GetActualDataLength() !=
            static_cast<LONG>(kPcmBytesPerFrame) ||
        output_sample.GetTime(&output_start, &output_end) != S_OK ||
        output_start != frame_time(kDiscardedPrerollFrames) ||
        output_sample.IsDiscontinuity() != S_OK) {
        std::fprintf(
            stderr,
            "post-flush restart mismatch: hr=0x%08lX len=%ld "
            "start=%lld disc=%d\n",
            hr, output_sample.GetActualDataLength(),
            output_start, output_sample.IsDiscontinuity() == S_OK);
        return 1;
    }

    return 0;
}

int run_budget_test(CMADFilterReloaded* filter, WORD channels = 2) {
    CMediaType input_type = make_mpeg_audio_type(channels);
    CMediaType output_type = make_pcm_type(channels, 44100);
    filter->SetMediaType(PINDIR_INPUT, &input_type);
    filter->SetMediaType(PINDIR_OUTPUT, &output_type);
    filter->BeginFlush();
    filter->EndFlush();
    filter->NewSegment(
        1000 * kReferenceTimePerSecond,
        1001 * kReferenceTimePerSecond, 1.0);
    constexpr size_t kBudgetFrames = 100;
    constexpr size_t kOutputBytes = 100 * kPcmBytesPerFrame;

    std::vector<uint8_t> input =
        make_silent_layer2_frames(kBudgetFrames, channels);
    std::vector<uint8_t> output(kOutputBytes);

    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("budget allocator"), nullptr, &hr);
    if (FAILED(hr)) {
        std::fprintf(stderr, "budget allocator construction failed\n");
        return 1;
    }

    CMediaSample input_sample(
        NAME("budget input"), &allocator, &hr,
        input.data(), static_cast<LONG>(input.size()));
    CMediaSample output_sample(
        NAME("budget output"), &allocator, &hr,
        output.data(), static_cast<LONG>(output.size()));

    hr = filter->Transform(&input_sample, &output_sample);
    const LONG first_length = output_sample.GetActualDataLength();
    if (hr != S_OK || first_length <= 0 ||
        first_length > static_cast<LONG>(
            (64 - kDiscardedPrerollFrames) * kPcmBytesPerFrame * channels / 2)) {
        std::fprintf(
            stderr,
            "decode budget was not applied: hr=0x%08lX len=%ld\n",
            hr, first_length);
        return 1;
    }

    uint8_t extra = 0;
    input_sample.SetPointer(&extra, 1);
    input_sample.SetActualDataLength(1);
    output_sample.SetActualDataLength(0);
    hr = filter->Transform(&input_sample, &output_sample);
    if (hr != S_OK) {
        std::fprintf(stderr, "budget continuation failed: 0x%08lX\n", hr);
        return 1;
    }

    return 0;
}

int run_continuation_test(CMADFilterReloaded* filter) {
    constexpr size_t kSegmentFrames = 10;
    constexpr size_t kOutputBytes = 24 * kPcmBytesPerFrame;
    const REFERENCE_TIME base = 100 * kReferenceTimePerSecond;

    // Start from a clean, deliberately far-away segment.
    filter->BeginFlush();
    filter->EndFlush();
    filter->NewSegment(base, base + 30 * kReferenceTimePerSecond, 1.0);

    std::vector<uint8_t> segment =
        make_silent_layer2_frames(kSegmentFrames);
    std::vector<uint8_t> output(kOutputBytes);

    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("continuation allocator"), nullptr, &hr);
    if (FAILED(hr)) {
        std::fprintf(stderr, "continuation allocator failed\n");
        return 1;
    }

    CMediaSample input_sample(
        NAME("continuation input"), &allocator, &hr,
        segment.data(), static_cast<LONG>(segment.size()));
    CMediaSample output_sample(
        NAME("continuation output"), &allocator, &hr,
        output.data(), static_cast<LONG>(output.size()));

    const size_t diag_start = filter->DiagnosticCount();
    size_t total_frames = 0;

    hr = filter->Transform(&input_sample, &output_sample);
    if (hr != S_OK) {
        std::fprintf(stderr, "continuation segment 1 failed\n");
        return 1;
    }
    total_frames +=
        static_cast<size_t>(output_sample.GetActualDataLength()) /
        kPcmBytesPerFrame;

    // The source flushes and starts a contiguous segment 10 frames later.
    filter->BeginFlush();
    filter->EndFlush();
    filter->NewSegment(
        base + kSegmentFrames * frame_time(1),
        base + 30 * kReferenceTimePerSecond, 1.0);

    output_sample.SetActualDataLength(0);
    hr = filter->Transform(&input_sample, &output_sample);
    if (hr != S_OK) {
        std::fprintf(stderr, "continuation segment 2 failed\n");
        return 1;
    }
    total_frames +=
        static_cast<size_t>(output_sample.GetActualDataLength()) /
        kPcmBytesPerFrame;

    // The final frame is held by libmad's guard until more data/EOS.
    const size_t expected_frames = 2 * kSegmentFrames - kBufferedTailFrames;
    if (total_frames != expected_frames) {
        std::fprintf(
            stderr, "continuation lost frames: got %zu, expected %zu\n",
            total_frames, expected_frames);
        return 1;
    }

    for (size_t i = diag_start; i < filter->DiagnosticCount(); ++i) {
        if (filter->DiagnosticAt(i).code == madfilter::DIAG_DECODE_ERROR) {
            std::fprintf(stderr, "continuation produced a decode error\n");
            return 1;
        }
    }

    return 0;
}

int run_micro_segment_test(CMADFilterReloaded* filter) {
    filter->Stop();
    constexpr size_t kFrames = 30;
    constexpr size_t kOutputBytes = kFrames * kPcmBytesPerFrame;
    const REFERENCE_TIME base = 300 * kReferenceTimePerSecond;

    std::vector<uint8_t> one_frame =
        make_silent_layer2_frames(1);
    std::vector<uint8_t> output(kOutputBytes);

    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("micro segment allocator"), nullptr, &hr);
    if (FAILED(hr)) return 1;

    CMediaSample input_sample(
        NAME("micro segment input"), &allocator, &hr,
        one_frame.data(), static_cast<LONG>(one_frame.size()));
    CMediaSample output_sample(
        NAME("micro segment output"), &allocator, &hr,
        output.data(), static_cast<LONG>(output.size()));

    const size_t diag_start = filter->DiagnosticCount();
    size_t total_frames = 0;

    for (size_t i = 0; i < kFrames; ++i) {
        const REFERENCE_TIME tStart = base + i * frame_time(1);
        filter->BeginFlush();
        filter->EndFlush();
        filter->NewSegment(
            tStart, tStart + frame_time(1), 1.0);

        output_sample.SetActualDataLength(0);
        hr = filter->Transform(&input_sample, &output_sample);
        if (hr != S_OK) {
            std::fprintf(stderr, "micro segment %zu failed\n", i);
            return 1;
        }
        total_frames +=
            static_cast<size_t>(output_sample.GetActualDataLength()) /
            kPcmBytesPerFrame;
    }

    size_t resets = 0;
    size_t continuations = 0;
    for (size_t i = diag_start; i < filter->DiagnosticCount(); ++i) {
        const uint32_t code = filter->DiagnosticAt(i).code;
        if (code == madfilter::DIAG_SEGMENT_RESET) ++resets;
        if (code == madfilter::DIAG_CONTINUATION) ++continuations;
        if (code == madfilter::DIAG_DECODE_ERROR) {
            std::fprintf(stderr, "micro segment decode error\n");
            return 1;
        }
    }

    if (resets != 1 || continuations < kFrames - 2 ||
        total_frames != kFrames - kBufferedTailFrames) {
        std::fprintf(
            stderr,
            "micro segment mismatch: resets=%zu continuations=%zu "
            "frames=%zu\n",
            resets, continuations, total_frames);
        return 1;
    }

    return 0;
}

int run_seek_test(CMADFilterReloaded* filter) {
    // A normal seek supplies an absolute segment position and sample time
    // zero. Short forward seeks must discard carry just like long seeks.
    filter->Stop();
    CMediaType input_type = make_mpeg_audio_type();
    CMediaType output_type = make_pcm_type(2, 44100);
    filter->SetMediaType(PINDIR_INPUT, &input_type);
    filter->SetMediaType(PINDIR_OUTPUT, &output_type);
    std::vector<uint8_t> input = make_silent_layer2_frames(kFrameCount);
    std::vector<uint8_t> output(kPcmBytesPerFrame);
    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("seek allocator"), nullptr, &hr);
    if (FAILED(hr)) return 1;
    CMediaSample in(NAME("seek input"), &allocator, &hr,
                    input.data(), static_cast<LONG>(input.size()));
    CMediaSample out(NAME("seek output"), &allocator, &hr,
                     output.data(), static_cast<LONG>(output.size()));
    for (int i = 0; i < 3; ++i) {
        filter->BeginFlush();
        filter->EndFlush();
        const REFERENCE_TIME start = (5LL + i) * kReferenceTimePerSecond;
        filter->NewSegment(start, start + kReferenceTimePerSecond, 1.0);
        REFERENCE_TIME zero = 0, end = kReferenceTimePerSecond;
        in.SetTime(i == 2 ? nullptr : &zero, i == 2 ? nullptr : &end);
        in.SetDiscontinuity(TRUE);
        const size_t diag_start = filter->DiagnosticCount();
        hr = filter->Transform(&in, &out);
        REFERENCE_TIME actual_start = 0, actual_end = 0;
        if (hr != S_OK || out.GetTime(&actual_start, &actual_end) != S_OK ||
            actual_start != frame_time(kDiscardedPrerollFrames) ||
            actual_end != frame_time(kDiscardedPrerollFrames + 1) ||
            out.IsDiscontinuity() != S_OK) {
            std::fprintf(stderr, "seek %d retained old audio or wrong clock\n", i);
            return 1;
        }
        for (size_t j = diag_start; j < filter->DiagnosticCount(); ++j) {
            const uint32_t code = filter->DiagnosticAt(j).code;
            if (code == madfilter::DIAG_CONTINUATION ||
                code == madfilter::DIAG_SOURCE_BYTE_CLOCK) {
                std::fprintf(stderr, "normal seek enabled BSAudio policy\n");
                return 1;
            }
        }
    }
    return 0;
}

int run_format_restart_test(CMADFilterReloaded* filter) {
    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("format restart allocator"), nullptr, &hr);
    if (FAILED(hr)) return 1;
    std::vector<uint8_t> output(192 * 1024);
    for (WORD channels = 1; channels <= 2; ++channels) {
        filter->Stop();
        CMediaType input_type = make_mpeg_audio_type(channels);
        CMediaType output_type = make_pcm_type(channels, 44100);
        filter->SetMediaType(PINDIR_INPUT, &input_type);
        filter->SetMediaType(PINDIR_OUTPUT, &output_type);
        filter->NewSegment(0, kReferenceTimePerSecond, 1.0);
        std::vector<uint8_t> input = make_silent_layer2_frames(8, channels);
        CMediaSample in(NAME("format restart input"), &allocator, &hr,
                        input.data(), static_cast<LONG>(input.size()));
        CMediaSample out(NAME("format restart output"), &allocator, &hr,
                         output.data(), static_cast<LONG>(output.size()));
        REFERENCE_TIME zero = 0, end = frame_time(8);
        in.SetTime(&zero, &end);
        const size_t diag_start = filter->DiagnosticCount();
        hr = filter->Transform(&in, &out);
        REFERENCE_TIME actual_start = 0, actual_end = 0;
        if (hr != S_OK || out.GetTime(&actual_start, &actual_end) != S_OK ||
            actual_start != frame_time(kDiscardedPrerollFrames) ||
            actual_end != frame_time(7) ||
            out.GetActualDataLength() !=
                static_cast<LONG>(3 * kPcmBytesPerFrame * channels / 2)) {
            std::fprintf(stderr, "format restart retained old channel count\n");
            return 1;
        }
        for (size_t j = diag_start; j < filter->DiagnosticCount(); ++j) {
            if (filter->DiagnosticAt(j).code == madfilter::DIAG_FORMAT_MISMATCH) {
                std::fprintf(stderr, "valid format restart reported mismatch\n");
                return 1;
            }
        }
    }
    return 0;
}

int run_sample_tests(CMADFilterReloaded* filter) {
    std::vector<uint8_t> input = make_silent_layer2_frames(kFrameCount);
    std::vector<uint8_t> output(kFrameCount * kPcmBytesPerFrame);

    HRESULT hr = S_OK;
    CMemAllocator allocator(NAME("filter_sample_test allocator"), nullptr, &hr);
    if (FAILED(hr)) {
        std::fprintf(stderr, "sample allocator construction failed: 0x%08lX\n", hr);
        return 1;
    }

    CMediaSample input_sample(
        NAME("filter_sample_test input"), &allocator, &hr,
        input.data(), static_cast<LONG>(input.size()));
    if (FAILED(hr)) {
        std::fprintf(stderr, "input sample construction failed: 0x%08lX\n", hr);
        return 1;
    }

    CMediaSample output_sample(
        NAME("filter_sample_test output"), &allocator, &hr,
        output.data(), static_cast<LONG>(output.size()));
    if (FAILED(hr)) {
        std::fprintf(stderr, "output sample construction failed: 0x%08lX\n", hr);
        return 1;
    }

    const REFERENCE_TIME stream_start = 3 * kReferenceTimePerSecond;
    REFERENCE_TIME input_start = stream_start;
    REFERENCE_TIME input_end = stream_start + frame_time(kFrameCount);
    input_sample.SetTime(&input_start, &input_end);

    hr = filter->Transform(&input_sample, &output_sample);
    if (hr != S_OK) {
        std::fprintf(stderr, "oversized input transform failed: 0x%08lX\n", hr);
        return 1;
    }

    const LONG expected_bytes = static_cast<LONG>(
        (kFrameCount - kDiscardedPrerollFrames - kBufferedTailFrames) *
        kPcmBytesPerFrame);
    if (output_sample.GetActualDataLength() != expected_bytes) {
        std::fprintf(
            stderr, "partial input was lost: expected %ld PCM bytes, got %ld\n",
            expected_bytes, output_sample.GetActualDataLength());
        return 1;
    }

    if (output_sample.IsDiscontinuity() != S_OK) {
        std::fprintf(stderr, "first PCM output was not marked discontinuous\n");
        return 1;
    }

    REFERENCE_TIME output_start = 0;
    REFERENCE_TIME output_end = 0;
    if (output_sample.GetTime(&output_start, &output_end) != S_OK ||
        output_start != stream_start + frame_time(kDiscardedPrerollFrames) ||
        output_end != stream_start + frame_time(
            kFrameCount - kBufferedTailFrames)) {
        std::fprintf(
            stderr, "first PCM timestamp mismatch: start=%lld end=%lld\n",
            output_start, output_end);
        return 1;
    }

    input = make_silent_layer2_frames(1);
    output.assign(kPcmBytesPerFrame, 0);
    input_sample.SetPointer(input.data(), static_cast<LONG>(input.size()));
    input_sample.SetActualDataLength(static_cast<LONG>(input.size()));
    output_sample.SetPointer(output.data(), static_cast<LONG>(output.size()));
    output_sample.SetActualDataLength(0);
    output_sample.SetDiscontinuity(FALSE);
    input_start = stream_start + frame_time(kFrameCount);
    input_end = stream_start + frame_time(kFrameCount + 1);
    input_sample.SetTime(&input_start, &input_end);

    hr = filter->Transform(&input_sample, &output_sample);
    if (hr != S_OK ||
        output_sample.GetActualDataLength() != static_cast<LONG>(kPcmBytesPerFrame)) {
        std::fprintf(stderr, "follow-up sample did not decode exactly one frame\n");
        return 1;
    }

    if (output_sample.IsDiscontinuity() == S_OK) {
        std::fprintf(stderr, "discontinuity leaked onto the second PCM sample\n");
        return 1;
    }

    if (output_sample.GetTime(&output_start, &output_end) != S_OK ||
        output_start != stream_start + frame_time(kFrameCount - 1) ||
        output_end != stream_start + frame_time(kFrameCount)) {
        std::fprintf(
            stderr, "buffered PCM timestamp mismatch: start=%lld end=%lld\n",
            output_start, output_end);
        return 1;
    }

    FilterOwner<TestSource> source(
        new (std::nothrow) TestSource(&hr));
    if (!source.operator->() || FAILED(hr)) {
        std::fprintf(stderr, "test source construction failed: 0x%08lX\n", hr);
        return 1;
    }

    FilterOwner<CapturingSink> sink(
        new (std::nothrow) CapturingSink(&hr));
    if (!sink.operator->() || FAILED(hr)) {
        std::fprintf(stderr, "test sink construction failed: 0x%08lX\n", hr);
        return 1;
    }

    CBasePin* source_pin = source->GetPin(0);
    CBasePin* filter_input_pin = filter->GetPin(0);
    CBasePin* output_pin = filter->GetPin(1);
    CBasePin* sink_pin = sink->GetPin(0);
    if (!source_pin || !filter_input_pin || !output_pin || !sink_pin) {
        std::fprintf(stderr, "failed to create EOF test pins\n");
        return 1;
    }

    hr = source_pin->Connect(
        filter_input_pin, &source->OutputPin()->MediaType());
    if (FAILED(hr)) {
        std::fprintf(stderr, "failed to connect EOF test source: 0x%08lX\n", hr);
        return 1;
    }

    CMediaType output_type;
    hr = filter->GetMediaType(0, &output_type);
    if (FAILED(hr)) {
        std::fprintf(stderr, "failed to get decoded media type: 0x%08lX\n", hr);
        source_pin->Disconnect();
        filter_input_pin->Disconnect();
        return 1;
    }

    hr = output_pin->Connect(sink_pin, &output_type);
    if (FAILED(hr)) {
        std::fprintf(stderr, "failed to connect EOF test sink: 0x%08lX\n", hr);
        source_pin->Disconnect();
        filter_input_pin->Disconnect();
        return 1;
    }

    auto stop_and_disconnect = [&]() {
        source->Stop();
        filter->Stop();
        sink->Stop();
        output_pin->Disconnect();
        sink_pin->Disconnect();
        source_pin->Disconnect();
        filter_input_pin->Disconnect();
    };

    hr = sink->Run(0);
    if (SUCCEEDED(hr)) {
        hr = filter->Run(0);
    }
    if (SUCCEEDED(hr)) {
        hr = source->Run(0);
    }
    if (FAILED(hr)) {
        std::fprintf(stderr, "failed to run EOF test filters: 0x%08lX\n", hr);
        stop_and_disconnect();
        return 1;
    }

    hr = filter->EndOfStream();
    if (FAILED(hr)) {
        std::fprintf(stderr, "filter EndOfStream failed: 0x%08lX\n", hr);
        stop_and_disconnect();
        return 1;
    }

    if (sink->sample_count != 1 ||
        sink->pcm_bytes != static_cast<LONG>(kPcmBytesPerFrame) ||
        sink->discontinuous ||
        !sink->eos_received ||
        !sink->time_valid ||
        sink->time_start != stream_start + frame_time(kFrameCount) ||
        sink->time_end != stream_start + frame_time(kFrameCount + 1)) {
        std::fprintf(
            stderr,
            "EOF tail mismatch: samples=%d bytes=%ld disc=%d eos=%d "
            "time=%d [%lld,%lld]\n",
            sink->sample_count, sink->pcm_bytes,
            sink->discontinuous ? 1 : 0, sink->eos_received ? 1 : 0,
            sink->time_valid ? 1 : 0, sink->time_start, sink->time_end);
        stop_and_disconnect();
        return 1;
    }

    const REFERENCE_TIME segment_start = 9 * kReferenceTimePerSecond;
    hr = filter->NewSegment(
        segment_start, segment_start + 10 * kReferenceTimePerSecond, 1.0);
    if (FAILED(hr)) {
        std::fprintf(stderr, "timestamp reset NewSegment failed: 0x%08lX\n", hr);
        stop_and_disconnect();
        return 1;
    }

    input = make_silent_layer2_frames(kFrameCount);
    output.assign(kFrameCount * kPcmBytesPerFrame, 0);
    input_sample.SetPointer(input.data(), static_cast<LONG>(input.size()));
    input_sample.SetActualDataLength(static_cast<LONG>(input.size()));
    input_sample.SetTime(nullptr, nullptr);
    input_sample.SetDiscontinuity(TRUE);
    output_sample.SetPointer(output.data(), static_cast<LONG>(output.size()));
    output_sample.SetActualDataLength(0);
    output_sample.SetTime(nullptr, nullptr);
    output_sample.SetDiscontinuity(FALSE);

    hr = filter->Transform(&input_sample, &output_sample);
    if (hr != S_OK ||
        output_sample.GetTime(&output_start, &output_end) != S_OK ||
        output_start != frame_time(kDiscardedPrerollFrames) ||
        output_end != frame_time(
            kFrameCount - kBufferedTailFrames) ||
        output_sample.IsDiscontinuity() != S_OK) {
        std::fprintf(
            stderr,
            "NewSegment timestamp reset mismatch: hr=0x%08lX [%lld,%lld]\n",
            hr, output_start, output_end);
        stop_and_disconnect();
        return 1;
    }

    stop_and_disconnect();
    return 0;
}

} // namespace

int run_eof_format_rejection_test() {
    SetEnvironmentVariableW(L"MADFILTER_BSAUDIO", L"1");
    HRESULT hr = S_OK;
    FilterOwner<CMADFilterReloaded> filter(static_cast<CMADFilterReloaded*>(
        CMADFilterReloaded::CreateInstance(nullptr, &hr)));
    SetEnvironmentVariableW(L"MADFILTER_BSAUDIO", nullptr);
    FilterOwner<TestSource> source(new TestSource(&hr));
    FilterOwner<CapturingSink> sink(new CapturingSink(&hr));
    auto* source_pin = source->GetPin(0);
    auto* input_pin = filter->GetPin(0);
    auto* output_pin = filter->GetPin(1);
    auto* sink_pin = sink->GetPin(0);
    auto disconnect = [&]() {
        source->Stop(); filter->Stop(); sink->Stop();
        source_pin->Disconnect(); input_pin->Disconnect();
        output_pin->Disconnect(); sink_pin->Disconnect();
    };
    hr = source_pin->Connect(input_pin, &source->OutputPin()->MediaType());
    CMediaType pcm_type = make_pcm_type(2, 44100);
    if (SUCCEEDED(hr)) hr = output_pin->Connect(sink_pin, &pcm_type);
    if (SUCCEEDED(hr)) hr = sink->Run(0);
    if (SUCCEEDED(hr)) hr = filter->Run(0);
    if (SUCCEEDED(hr)) hr = source->Run(0);
    if (FAILED(hr)) { disconnect(); return 1; }

    CMemAllocator allocator(NAME("EOF mismatch allocator"), nullptr, &hr);
    auto input = make_silent_layer2_frames(1, 1);
    input[1] = 0xfb; input[2] = 0x90;
    std::vector<uint8_t> output(192 * 1024);
    CMediaSample in(NAME("EOF mismatch input"), &allocator, &hr,
        input.data(), static_cast<LONG>(input.size()));
    CMediaSample out(NAME("EOF mismatch output"), &allocator, &hr,
        output.data(), static_cast<LONG>(output.size()));
    filter->NewSegment(0, kReferenceTimePerSecond, 1.0);
    hr = filter->Transform(&in, &out);
    const bool buffered = hr == S_OK && out.GetActualDataLength() == 0;
    hr = filter->EndOfStream();
    const bool rejected = hr == VFW_E_TYPE_NOT_ACCEPTED && sink->sample_count == 0;
    disconnect();
    if (!buffered || !rejected) {
        std::fprintf(stderr, "EOF delivered PCM with the wrong channel format\n");
        return 1;
    }
    return 0;
}

int run_format_rejection_tests() {
    for (bool bsa : {false, true}) {
        SetEnvironmentVariableW(L"MADFILTER_BSAUDIO", bsa ? L"1" : nullptr);
        for (bool unknown_format : {false, true}) {
            HRESULT hr = S_OK;
            FilterOwner<CMADFilterReloaded> filter(static_cast<CMADFilterReloaded*>(
                CMADFilterReloaded::CreateInstance(nullptr, &hr)));
            CMemAllocator allocator(NAME("format rejection allocator"), nullptr, &hr);
            CMediaType input_type = make_mpeg_audio_type(unknown_format ? 0 : 2,
                                                        unknown_format ? 0 : 44100);
            CMediaType output_type = make_pcm_type(2, 44100);
            filter->SetMediaType(PINDIR_INPUT, &input_type);
            filter->SetMediaType(PINDIR_OUTPUT, &output_type);
            filter->NewSegment(0, kReferenceTimePerSecond, 1.0);
            auto input = make_silent_layer2_frames(10, 1);
            for (size_t i = 0; i < 10; ++i) {
                input[i * kFrameBytes + 1] = 0xfb;
                input[i * kFrameBytes + 2] = 0x90;
            }
            std::vector<uint8_t> output(192 * 1024);
            CMediaSample in(NAME("mismatch input"), &allocator, &hr,
                input.data(), static_cast<LONG>(input.size()));
            CMediaSample out(NAME("mismatch output"), &allocator, &hr,
                output.data(), static_cast<LONG>(output.size()));
            if (filter->Transform(&in, &out) != VFW_E_TYPE_NOT_ACCEPTED) {
                std::fprintf(stderr, "negotiated stereo accepted mono MP3\n");
                SetEnvironmentVariableW(L"MADFILTER_BSAUDIO", nullptr);
                return 1;
            }
        }
    }
    SetEnvironmentVariableW(L"MADFILTER_BSAUDIO", nullptr);
    return run_eof_format_rejection_test();
}

int main() {
    SetEnvironmentVariableW(L"MADFILTER_BSAUDIO", nullptr);
    SetEnvironmentVariableW(L"MADFILTER_NO_TIMESTAMPS", nullptr);
    HRESULT co_hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(co_hr)) {
        std::fprintf(stderr, "COM initialization failed: 0x%08lX\n", co_hr);
        return 1;
    }

    HRESULT hr = S_OK;
    CUnknown* unknown = CMADFilterReloaded::CreateInstance(nullptr, &hr);
    if (!unknown || FAILED(hr)) {
        std::fprintf(stderr, "filter construction failed: 0x%08lX\n", hr);
        CoUninitialize();
        return 1;
    }

    unknown->NonDelegatingAddRef();
    CMADFilterReloaded* filter =
        static_cast<CMADFilterReloaded*>(unknown);
    int result = run_format_rejection_tests();
    if (result == 0) result = run_format_tests(filter);
    if (result == 0) {
        result = run_buffer_size_test(filter);
    }
    if (result == 0) {
        result = run_sample_tests(filter);
    }
    if (result == 0) {
        HRESULT carry_hr = S_OK;
        CUnknown* carry_unknown =
            CMADFilterReloaded::CreateInstance(nullptr, &carry_hr);
        if (!carry_unknown || FAILED(carry_hr)) {
            std::fprintf(stderr, "carry filter construction failed: 0x%08lX\n",
                         carry_hr);
            result = 1;
        } else {
            carry_unknown->NonDelegatingAddRef();
            CMADFilterReloaded* carry_filter =
                static_cast<CMADFilterReloaded*>(carry_unknown);
            result = run_carry_test(carry_filter);
            if (result == 0) {
                result = run_lifecycle_test(carry_filter);
            }
            if (result == 0) {
                result = run_budget_test(carry_filter);
            }
            if (result == 0) {
                result = run_budget_test(carry_filter, 1);
            }
            if (result == 0) {
                result = run_seek_test(carry_filter);
            }
            if (result == 0) {
                result = run_format_restart_test(carry_filter);
            }
            carry_unknown->NonDelegatingRelease();
        }
    }
    if (result == 0) {
        SetEnvironmentVariableW(L"MADFILTER_BSAUDIO", L"1");
        CUnknown* bsa_unknown = CMADFilterReloaded::CreateInstance(nullptr, &hr);
        SetEnvironmentVariableW(L"MADFILTER_BSAUDIO", nullptr);
        if (!bsa_unknown || FAILED(hr)) {
            result = 1;
        } else {
            bsa_unknown->NonDelegatingAddRef();
            auto* bsa_filter = static_cast<CMADFilterReloaded*>(bsa_unknown);
            result = run_continuation_test(bsa_filter);
            if (result == 0) result = run_micro_segment_test(bsa_filter);
            bsa_unknown->NonDelegatingRelease();
        }
    }
    unknown->NonDelegatingRelease();
    CoUninitialize();
    return result;
}
