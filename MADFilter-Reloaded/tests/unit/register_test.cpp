#include <windows.h>

// Exercise the real registration error path without changing the registry.
static DWORD registration_exit_code = 0;
static BOOL WINAPI fake_create_process(
    LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL,
    DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION info) {
    info->hProcess = reinterpret_cast<HANDLE>(1);
    info->hThread = reinterpret_cast<HANDLE>(2);
    return TRUE;
}
static DWORD WINAPI fake_wait(HANDLE, DWORD) { return WAIT_OBJECT_0; }
static BOOL WINAPI fake_exit_code(HANDLE, LPDWORD code) {
    *code = registration_exit_code;
    return TRUE;
}
static BOOL WINAPI fake_close(HANDLE) { return TRUE; }

#define CreateProcessW fake_create_process
#define WaitForSingleObject fake_wait
#define GetExitCodeProcess fake_exit_code
#define CloseHandle fake_close
#define main registration_utility_main
#include "../../src/tools/MADFilterRegister.cpp"
#undef main
#undef CreateProcessW
#undef WaitForSingleObject
#undef GetExitCodeProcess
#undef CloseHandle

int main() {
    const WCHAR* arguments[] = {
        L"", L"ordinary", L"with spaces", L"C:\\Games\\",
        L"before\"after", L"before\\\"after", L"before\\\\\"after"
    };
    for (const auto* argument : arguments) {
        WCHAR command[2048] = L"dummy.exe ";
        if (!append_quoted_argument(command, _countof(command), argument)) return 1;
        int argc = 0;
        auto argv = CommandLineToArgvW(command, &argc);
        const bool match = argv && argc == 2 && wcscmp(argv[1], argument) == 0;
        if (argv) LocalFree(argv);
        if (!match) {
            std::fprintf(stderr, "launch argument did not round-trip\n");
            return 1;
        }
    }
    if (unregister_filter() != 0) return 1;
    registration_exit_code = 5;
    if (unregister_filter() == 0) {
        std::fprintf(stderr, "failed unregistration returned success\n");
        return 1;
    }
    return 0;
}
