#include <windows.h>
#include <unknwn.h>

#include <cstdio>
#include <cwchar>

namespace {

const GUID CLSID_MADFilterReloaded = {
    0x4fac742e, 0x1b8b, 0x4de8,
    {0xad, 0x7b, 0xd0, 0x15, 0x8f, 0x7c, 0x24, 0x48}
};

using DllGetClassObjectFn =
    HRESULT (STDAPICALLTYPE*)(REFCLSID, REFIID, LPVOID*);
using DllCanUnloadNowFn = HRESULT (STDAPICALLTYPE*)();

} // namespace

int wmain(int argc, wchar_t* argv[]) {
    if (argc != 2) {
        std::fwprintf(stderr, L"Usage: filter_load_test <filter.ax>\n");
        return 2;
    }

    HMODULE module = LoadLibraryExW(
        argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) {
        std::fwprintf(stderr, L"LoadLibraryExW failed: %lu\n", GetLastError());
        return 3;
    }

    auto get_class_object = reinterpret_cast<DllGetClassObjectFn>(
        GetProcAddress(module, "DllGetClassObject"));
    auto can_unload = reinterpret_cast<DllCanUnloadNowFn>(
        GetProcAddress(module, "DllCanUnloadNow"));
    if (!get_class_object || !can_unload) {
        std::fprintf(stderr, "Required COM exports are missing.\n");
        FreeLibrary(module);
        return 4;
    }

    IClassFactory* factory = nullptr;
    HRESULT hr = get_class_object(
        CLSID_MADFilterReloaded, IID_IClassFactory,
        reinterpret_cast<void**>(&factory));
    if (FAILED(hr) || !factory) {
        std::fprintf(stderr, "DllGetClassObject failed: 0x%08lX\n", hr);
        FreeLibrary(module);
        return 5;
    }

    IUnknown* instance = nullptr;
    hr = factory->CreateInstance(
        nullptr, IID_IUnknown, reinterpret_cast<void**>(&instance));
    if (FAILED(hr) || !instance) {
        std::fprintf(stderr, "CreateInstance failed: 0x%08lX\n", hr);
        factory->Release();
        FreeLibrary(module);
        return 6;
    }

    instance->Release();
    factory->Release();

    hr = can_unload();
    if (hr != S_OK) {
        std::fprintf(stderr, "DllCanUnloadNow returned: 0x%08lX\n", hr);
        FreeLibrary(module);
        return 7;
    }

    FreeLibrary(module);
    return 0;
}
