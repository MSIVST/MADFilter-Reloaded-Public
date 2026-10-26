#include <windows.h>
#include <dshow.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>

namespace {

template <typename T>
class ComPtr {
public:
    ComPtr() = default;
    ~ComPtr() {
        Reset();
    }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    T* Get() const {
        return pointer_;
    }

    T** GetAddressOf() {
        return &pointer_;
    }

    T** ReleaseAndGetAddressOf() {
        Reset();
        return &pointer_;
    }

    T* operator->() const {
        return pointer_;
    }

    explicit operator bool() const {
        return pointer_ != nullptr;
    }

    template <typename U>
    HRESULT As(ComPtr<U>* destination) const {
        if (!destination) {
            return E_POINTER;
        }
        if (!pointer_) {
            return E_NOINTERFACE;
        }
        return pointer_->QueryInterface(
            __uuidof(U),
            reinterpret_cast<void**>(destination->ReleaseAndGetAddressOf()));
    }

private:
    void Reset() {
        if (pointer_) {
            pointer_->Release();
            pointer_ = nullptr;
        }
    }

    T* pointer_ = nullptr;
};

const GUID CLSID_MADFilterReloaded = {
    0x4fac742e, 0x1b8b, 0x4de8,
    {0xad, 0x7b, 0xd0, 0x15, 0x8f, 0x7c, 0x24, 0x48}
};

constexpr LONGLONG REFERENCE_TIME_PER_SECOND = 10000000;

bool find_madfilter(
    IGraphBuilder* graph, ComPtr<IBaseFilter>* madfilter) {
    ComPtr<IEnumFilters> filters;
    HRESULT hr = graph->EnumFilters(filters.GetAddressOf());
    if (FAILED(hr)) {
        std::fprintf(stderr, "EnumFilters failed: 0x%08lX\n", hr);
        return false;
    }

    bool found = false;
    ComPtr<IBaseFilter> filter;
    while (filters->Next(1, filter.ReleaseAndGetAddressOf(), nullptr) == S_OK) {
        FILTER_INFO info = {};
        if (SUCCEEDED(filter->QueryFilterInfo(&info))) {
            CLSID clsid = {};
            WCHAR clsid_text[64] = {};
            if (SUCCEEDED(filter->GetClassID(&clsid))) {
                if (StringFromGUID2(
                        clsid, clsid_text, ARRAYSIZE(clsid_text)) == 0) {
                    clsid_text[0] = L'\0';
                }
                if (IsEqualGUID(clsid, CLSID_MADFilterReloaded)) {
                    found = true;
                    if (madfilter) {
                        filter.As(madfilter);
                    }
                }
            }

            std::fwprintf(
                stdout, L"  Filter: %ls [%ls]\n", info.achName, clsid_text);
            if (info.pGraph) {
                info.pGraph->Release();
            }
        }
    }

    return found;
}

bool get_output_format(
    IBaseFilter* filter, DWORD* rate, WORD* channels) {
    if (!filter || !rate || !channels) {
        return false;
    }

    ComPtr<IEnumPins> pins;
    if (FAILED(filter->EnumPins(pins.GetAddressOf()))) {
        return false;
    }

    ComPtr<IPin> pin;
    while (pins->Next(1, pin.ReleaseAndGetAddressOf(), nullptr) == S_OK) {
        PIN_DIRECTION direction = PINDIR_INPUT;
        if (FAILED(pin->QueryDirection(&direction)) ||
            direction != PINDIR_OUTPUT) {
            continue;
        }

        AM_MEDIA_TYPE media_type = {};
        if (FAILED(pin->ConnectionMediaType(&media_type))) {
            continue;
        }

        bool ok = false;
        if (media_type.majortype == MEDIATYPE_Audio &&
            media_type.subtype == MEDIASUBTYPE_PCM &&
            media_type.formattype == FORMAT_WaveFormatEx &&
            media_type.cbFormat >= sizeof(WAVEFORMATEX)) {
            const WAVEFORMATEX* wfx =
                reinterpret_cast<const WAVEFORMATEX*>(media_type.pbFormat);
            *rate = wfx->nSamplesPerSec;
            *channels = wfx->nChannels;
            ok = true;
        }

        CoTaskMemFree(media_type.pbFormat);
        return ok;
    }

    return false;
}

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 2 && argc != 4 && argc != 5) {
        std::fwprintf(
            stderr,
            L"Usage: filter_graph_test <input.mp3> [expected_rate "
            L"expected_channels [loops]]\n");
        return 2;
    }

    DWORD expected_rate = 0;
    WORD expected_channels = 0;
    int loops = 1;
    if (argc == 5) {
        loops = _wtoi(argv[4]);
        if (loops < 1 || loops > 1000) {
            std::fwprintf(stderr, L"Invalid loop count\n");
            return 2;
        }
    }
    if (argc >= 4) {
        expected_rate = static_cast<DWORD>(_wtoi(argv[2]));
        expected_channels = static_cast<WORD>(_wtoi(argv[3]));
        if (expected_rate == 0 || expected_channels == 0) {
            std::fwprintf(stderr, L"Invalid expected format\n");
            return 2;
        }
    }

    const DWORD attributes = GetFileAttributesW(argv[1]);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        std::fwprintf(stderr, L"Input file not found: %ls\n", argv[1]);
        return 3;
    }

    const HRESULT initialize_result =
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialize_result)) {
        std::fprintf(
            stderr, "CoInitializeEx failed: 0x%08lX\n", initialize_result);
        return 4;
    }

    int result = 0;
    for (int loop = 0; loop < loops && result == 0; ++loop) {
        if (loops > 1) {
            std::fwprintf(
                stdout, L"Loop %d/%d\n", loop + 1, loops);
        }
        {
        ComPtr<IGraphBuilder> graph;
        HRESULT hr = CoCreateInstance(
            CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(graph.GetAddressOf()));
        if (FAILED(hr)) {
            std::fprintf(
                stderr, "Filter graph creation failed: 0x%08lX\n", hr);
            result = 5;
        } else {
            hr = graph->RenderFile(argv[1], nullptr);
            if (FAILED(hr)) {
                std::fprintf(stderr, "RenderFile failed: 0x%08lX\n", hr);
                result = 6;
            } else {
                ComPtr<IBaseFilter> madfilter;
                const bool madfilter_found =
                    find_madfilter(graph.Get(), &madfilter);
                WCHAR allow_other[8] = {};
                const bool allow_other_filter =
                    GetEnvironmentVariableW(
                        L"MADFILTER_GRAPH_ALLOW_OTHER",
                        allow_other, _countof(allow_other)) > 0;

                if (!madfilter_found && !allow_other_filter) {
                    std::fprintf(
                        stderr,
                        "MADFilter Reloaded was not selected by Filter Mapper.\n");
                    result = 7;
                } else if (!madfilter_found) {
                    std::fwprintf(
                        stdout,
                        L"WARNING: MADFilter not selected; testing the "
                        L"system graph only.\n");
                } else if (expected_rate != 0) {
                    DWORD actual_rate = 0;
                    WORD actual_channels = 0;
                    if (!get_output_format(
                            madfilter.Get(), &actual_rate, &actual_channels)) {
                        std::fprintf(
                            stderr,
                            "MADFilter output pin has no PCM connection.\n");
                        result = 7;
                    } else if (actual_rate != expected_rate ||
                               actual_channels != expected_channels) {
                        std::fprintf(
                            stderr,
                            "Output format mismatch: got %lu Hz %u ch, "
                            "expected %lu Hz %u ch\n",
                            actual_rate, actual_channels,
                            expected_rate, expected_channels);
                        result = 7;
                    } else {
                        std::fprintf(
                            stdout, "Output: %lu Hz %u ch\n",
                            actual_rate, actual_channels);
                    }
                }
            }
        }

        ComPtr<IMediaControl> control;
        ComPtr<IMediaSeeking> seeking;
        if (result == 0) {
            hr = graph.As(&control);
            if (FAILED(hr)) {
                std::fprintf(
                    stderr, "IMediaControl unavailable: 0x%08lX\n", hr);
                result = 8;
            }
        }
        if (result == 0) {
            hr = graph.As(&seeking);
            if (FAILED(hr)) {
                std::fprintf(
                    stderr, "IMediaSeeking unavailable: 0x%08lX\n", hr);
                result = 9;
            }
        }

        ComPtr<IBasicAudio> audio;
        if (result == 0 && SUCCEEDED(graph.As(&audio))) {
            audio->put_Volume(-10000);
        }

        LONGLONG duration = 0;
        if (result == 0) {
            hr = seeking->GetDuration(&duration);
            if (FAILED(hr) || duration <= 0) {
                std::fprintf(
                    stderr, "GetDuration failed: 0x%08lX\n", hr);
                result = 10;
            }
        }

        OAFilterState state = State_Stopped;
        if (result == 0) {
            hr = control->Run();
            if (FAILED(hr)) {
                std::fprintf(stderr, "Run failed: 0x%08lX\n", hr);
                result = 11;
            } else {
                hr = control->GetState(5000, &state);
                if (FAILED(hr) || state != State_Running) {
                    std::fprintf(
                        stderr,
                        "Graph did not reach running state: 0x%08lX state=%d\n",
                        hr, static_cast<int>(state));
                    result = 12;
                }
            }
        }

        LONGLONG position_before_seek = 0;
        if (result == 0) {
            Sleep(500);
            hr = seeking->GetCurrentPosition(&position_before_seek);
            if (FAILED(hr) || position_before_seek <= 0) {
                std::fprintf(
                    stderr, "Playback position did not advance: 0x%08lX\n", hr);
                result = 13;
            }
        }

        const LONGLONG seek_target = duration / 2;
        if (result == 0) {
            hr = control->Pause();
            if (SUCCEEDED(hr)) {
                hr = control->GetState(5000, &state);
            }
            if (FAILED(hr) || state != State_Paused) {
                std::fprintf(
                    stderr,
                    "Graph did not reach paused state: 0x%08lX state=%d\n",
                    hr, static_cast<int>(state));
                result = 14;
            }
        }
        if (result == 0) {
            LONGLONG target = seek_target;
            hr = seeking->SetPositions(
                &target, AM_SEEKING_AbsolutePositioning,
                nullptr, AM_SEEKING_NoPositioning);
            if (FAILED(hr)) {
                std::fprintf(stderr, "Seek failed: 0x%08lX\n", hr);
                result = 15;
            }
        }
        if (result == 0) {
            hr = control->Run();
            if (SUCCEEDED(hr)) {
                hr = control->GetState(5000, &state);
            }
            if (FAILED(hr) || state != State_Running) {
                std::fprintf(
                    stderr,
                    "Graph did not resume after seek: 0x%08lX state=%d\n",
                    hr, static_cast<int>(state));
                result = 16;
            }
        }

        LONGLONG position_after_seek = 0;
        if (result == 0) {
            Sleep(500);
            hr = seeking->GetCurrentPosition(&position_after_seek);
            const LONGLONG seek_tolerance = 2 * REFERENCE_TIME_PER_SECOND;
            if (FAILED(hr) ||
                position_after_seek < seek_target - seek_tolerance) {
                std::fprintf(
                    stderr,
                    "Position after seek is outside tolerance: "
                    "0x%08lX position=%lld target=%lld\n",
                    hr, position_after_seek, seek_target);
                result = 17;
            }
        }

        if (control) {
            const HRESULT stop_result = control->Stop();
            if (result == 0 && FAILED(stop_result)) {
                std::fprintf(
                    stderr, "Stop failed: 0x%08lX\n", stop_result);
                result = 18;
            }
        }

        if (result == 0) {
            std::fprintf(
                stdout,
                "PASS: duration_ms=%lld before_seek_ms=%lld "
                "after_seek_ms=%lld\n",
                duration / 10000,
                position_before_seek / 10000,
                position_after_seek / 10000);
        }
        }
    }

    CoUninitialize();
    return result;
}
