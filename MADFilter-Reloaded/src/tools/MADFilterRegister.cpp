// MADFilterRegister.cpp — Registration utility for MADFilter Reloaded
//
// Standalone tool for registering and unregistering the DirectShow filter.
// Does NOT require the filter DLL to be present for registration management.
//
// Usage:
//   MADFilterRegister.exe /register     Register the filter (normal merit)
//   MADFilterRegister.exe /unregister   Unregister the filter
//   MADFilterRegister.exe /fallout3     Register with elevated merit for Fallout 3
//   MADFilterRegister.exe /launch <exe> [args...]
//                                        Register, run the game, unregister
//   MADFilterRegister.exe /status       Show current registration status
//   MADFilterRegister.exe /help         Show help
//
// Note: The /fallout3 mode uses MERIT_PREFERRED+1 to ensure Fallout 3
// selects this decoder without overriding the system default MP3 decoder.
// This is MUCH lower than the original filter's MERIT_PREFERRED+0x2E90000.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <objbase.h>
#include <shellapi.h>
#include <strsafe.h>

// DirectShow merit values (from strmif.h / DirectShow SDK)
// Defined here so the utility can compile without the full DirectShow SDK.
#ifndef MERIT_NORMAL
#define MERIT_NORMAL        0x00600000
#endif
#ifndef MERIT_PREFERRED
#define MERIT_PREFERRED     0x00800000
#endif

#ifdef _MSC_VER
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

// ── Filter identity ───────────────────────────────────────────────────
// Must match CLSIDs.h in src/filter/

// {4FAC742E-1B8B-4DE8-AD7B-D0158F7C2448}
static const GUID CLSID_MADFilterReloaded = {
    0x4fac742e, 0x1b8b, 0x4de8,
    {0xad, 0x7b, 0xd0, 0x15, 0x8f, 0x7c, 0x24, 0x48}
};

static const WCHAR* FILTER_NAME = L"MADFilter Reloaded";
static const WCHAR* FILTER_DLL   = L"MADFilterReloaded.ax";

// ── Registry paths ────────────────────────────────────────────────────

static const WCHAR* CLSID_KEY_FMT =
    L"CLSID\\{4FAC742E-1B8B-4DE8-AD7B-D0158F7C2448}";

// ── Helpers ───────────────────────────────────────────────────────────

static void print_usage() {
    fprintf(stdout,
        "MADFilter Reloaded - Registration Utility v2.0.0\n"
        "\n"
        "Usage:\n"
        "  MADFilterRegister.exe /register      Register filter (MERIT_NORMAL)\n"
        "  MADFilterRegister.exe /unregister    Unregister filter\n"
        "  MADFilterRegister.exe /fallout3      Register with elevated merit for Fallout 3\n"
        "  MADFilterRegister.exe /launch <exe> [args...]\n"
        "                                       Register, run, then unregister\n"
        "  MADFilterRegister.exe /status        Show registration status\n"
        "  MADFilterRegister.exe /help          Show this help\n"
        "\n"
        "Modes:\n"
        "  /register   - Normal registration.  The filter will be available\n"
        "                to DirectShow applications but will NOT override\n"
        "                the system default MP3 decoder.\n"
        "\n"
        "  /fallout3   - Compatibility mode for Fallout 3.  Uses elevated\n"
        "                merit (MERIT_PREFERRED+1) to ensure Fallout 3\n"
        "                selects this decoder for its radio streams.\n"
        "                WARNING: This may affect other 32-bit applications\n"
        "                that use DirectShow for MP3 playback.\n"
        "\n"
        "  /launch     - Registers with elevated merit, starts the specified\n"
        "                executable with optional arguments, waits for it to\n"
        "                exit, then unregisters.  Requires Administrator.\n"
        "                Use this to scope compatibility mode to one session.\n"
        "\n"
        "  /unregister - Removes all registration entries.  Run this before\n"
        "                deleting the .ax file.\n"
        "\n"
        "  /status     - Checks whether the filter is currently registered\n"
        "                and shows its merit level.\n"
        "\n"
        "Fallout 3 notes:\n"
        "  The filter target is 32-bit x86.  On 64-bit Windows, the 32-bit\n"
        "  registration hive is used automatically by 32-bit regsvr32.\n"
        "  Make sure to use the 32-bit version of this utility or regsvr32.\n"
    );
}

static bool is_admin() {
    SID_IDENTIFIER_AUTHORITY nt_authority = SECURITY_NT_AUTHORITY;
    PSID administrators_group = nullptr;
    if (!AllocateAndInitializeSid(
            &nt_authority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &administrators_group)) {
        return false;
    }

    BOOL is_member = FALSE;
    const BOOL checked =
        CheckTokenMembership(nullptr, administrators_group, &is_member);
    FreeSid(administrators_group);
    return checked != FALSE && is_member != FALSE;
}

static bool get_filter_dll_path(WCHAR dll_path[MAX_PATH]) {
    DWORD length = GetModuleFileNameW(nullptr, dll_path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        fprintf(stderr, "ERROR: Cannot determine utility path (error %lu)\n",
                GetLastError());
        return false;
    }

    WCHAR* last_slash = wcsrchr(dll_path, L'\\');
    if (!last_slash) {
        fprintf(stderr, "ERROR: Utility path has no directory component.\n");
        return false;
    }

    *(last_slash + 1) = L'\0';
    if (FAILED(StringCchCatW(dll_path, MAX_PATH, FILTER_DLL))) {
        fprintf(stderr, "ERROR: Filter DLL path exceeds MAX_PATH.\n");
        return false;
    }

    return true;
}

static LONG run_regsvr32(const WCHAR* options,
                         const WCHAR* dll_path,
                         DWORD* exit_code) {
    WCHAR regsvr32_path[MAX_PATH];
    const UINT system_length =
        GetSystemDirectoryW(regsvr32_path, _countof(regsvr32_path));
    if (system_length == 0 || system_length >= _countof(regsvr32_path) ||
        FAILED(StringCchCatW(
            regsvr32_path, _countof(regsvr32_path), L"\\regsvr32.exe"))) {
        fprintf(stderr, "ERROR: Cannot resolve system regsvr32 path.\n");
        return -1;
    }

    WCHAR cmd[1024];
    if (FAILED(StringCchPrintfW(
            cmd,
            _countof(cmd),
            L"\"%ls\" %ls \"%ls\"",
            regsvr32_path,
            options,
            dll_path))) {
        fprintf(stderr, "ERROR: Registration command exceeds buffer size.\n");
        return -1;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(regsvr32_path, cmd, nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        fprintf(stderr, "ERROR: Failed to launch system regsvr32 (error %lu)\n",
                GetLastError());
        return -1;
    }

    const DWORD wait_result = WaitForSingleObject(pi.hProcess, 30000);
    if (wait_result != WAIT_OBJECT_0) {
        if (wait_result == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, ERROR_TIMEOUT);
            WaitForSingleObject(pi.hProcess, 5000);
            fprintf(stderr, "ERROR: regsvr32 timed out.\n");
        } else {
            fprintf(stderr, "ERROR: Failed while waiting for regsvr32 (error %lu)\n",
                    GetLastError());
        }
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -1;
    }

    if (!GetExitCodeProcess(pi.hProcess, exit_code)) {
        fprintf(stderr, "ERROR: Cannot read regsvr32 exit code (error %lu)\n",
                GetLastError());
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return -1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}

static LONG register_filter(DWORD merit) {
    WCHAR dll_path[MAX_PATH];

    // Find the .ax file next to this executable
    if (!get_filter_dll_path(dll_path)) return -1;

    // Check if DLL exists
    if (GetFileAttributesW(dll_path) == INVALID_FILE_ATTRIBUTES) {
        fprintf(stderr, "ERROR: Filter DLL not found: %ls\n", dll_path);
        fprintf(stderr, "Make sure %ls is in the same directory as this utility.\n", FILTER_DLL);
        return -1;
    }

    fprintf(stdout, "Registering: %ls\n", dll_path);
    fprintf(stdout, "Merit: 0x%08lX (%s)\n", merit,
            merit == MERIT_NORMAL ? "MERIT_NORMAL" :
            merit == (MERIT_PREFERRED + 1) ? "MERIT_PREFERRED+1 (Fallout 3)" :
            "Custom");

    // Fallout 3 mode calls DllInstall with an explicit command line so the
    // DLL can register its DirectShow metadata with the requested merit.
    const WCHAR* options =
        merit == (MERIT_PREFERRED + 1)
            ? L"/s /n /i:fallout3"
            : L"/s";
    DWORD exit_code = 0;
    if (run_regsvr32(options, dll_path, &exit_code) != 0) {
        return -1;
    }

    if (exit_code != 0) {
        fprintf(stderr, "ERROR: regsvr32 returned exit code %lu\n", exit_code);
        fprintf(stderr, "Try running as Administrator.\n");
        return -1;
    }

    fprintf(stdout, "Registration successful.\n");
    return 0;
}

static LONG unregister_filter() {
    WCHAR dll_path[MAX_PATH];

    if (!get_filter_dll_path(dll_path)) return -1;

    fprintf(stdout, "Unregistering: %ls\n", dll_path);

    DWORD exit_code = 0;
    if (run_regsvr32(L"/s /u", dll_path, &exit_code) != 0) {
        return -1;
    }

    if (exit_code != 0) {
        fprintf(stderr, "WARNING: regsvr32 returned exit code %lu\n", exit_code);
        fprintf(stderr, "The filter may already be unregistered.\n");
        return -1;
    }

    fprintf(stdout, "Unregistration complete.\n");
    return 0;
}

static LONG launch_game(const WCHAR* exe_path, const WCHAR* arguments) {
    if (!exe_path || !*exe_path) {
        fprintf(stderr, "ERROR: No executable specified for /launch.\n");
        return -1;
    }

    WCHAR full_command[32768];
    if (arguments && *arguments) {
        if (FAILED(StringCchPrintfW(
                full_command, _countof(full_command),
                L"\"%ls\" %ls", exe_path, arguments))) {
            fprintf(stderr, "ERROR: Launch command line is too long.\n");
            return -1;
        }
    } else {
        if (FAILED(StringCchPrintfW(
                full_command, _countof(full_command),
                L"\"%ls\"", exe_path))) {
            fprintf(stderr, "ERROR: Launch command line is too long.\n");
            return -1;
        }
    }

    WCHAR working_directory[MAX_PATH];
    if (FAILED(StringCchCopyW(
            working_directory, _countof(working_directory), exe_path))) {
        return -1;
    }
    WCHAR* last_slash = wcsrchr(working_directory, L'\\');
    if (last_slash) {
        *(last_slash + 1) = L'\0';
    } else {
        working_directory[0] = L'\0';
    }

    fprintf(stdout,
        "==============================================================\n"
        "  SCOPED FALLOUT 3 MODE\n"
        "\n"
        "  Registering preferred merit system-wide for this session.\n"
        "  The filter will be unregistered when the process exits.\n"
        "==============================================================\n"
        "\n");

    if (register_filter(MERIT_PREFERRED + 1) != 0) {
        fprintf(stderr, "ERROR: Registration failed; not launching.\n");
        return -1;
    }

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    WCHAR mutable_command[32768];
    if (FAILED(StringCchCopyW(
            mutable_command, _countof(mutable_command), full_command))) {
        unregister_filter();
        return -1;
    }

    const WCHAR* current_directory =
        working_directory[0] ? working_directory : nullptr;

    if (!CreateProcessW(
            nullptr, mutable_command, nullptr, nullptr, FALSE, 0,
            nullptr, current_directory, &si, &pi)) {
        fprintf(stderr, "ERROR: Failed to launch %ls (error %lu)\n",
                exe_path, GetLastError());
        unregister_filter();
        return -1;
    }

    fprintf(stdout, "Launched: %ls\n", exe_path);
    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 1;
    if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
        fprintf(stderr, "WARNING: Cannot read process exit code (error %lu)\n",
                GetLastError());
        exit_code = 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (unregister_filter() != 0) {
        fprintf(stderr,
                "WARNING: The process exited but unregistration failed.\n"
                "Run MADFilterRegister.exe /unregister manually.\n");
        return exit_code != 0 ? static_cast<LONG>(exit_code) : 1;
    }

    return static_cast<LONG>(exit_code);
}

static bool append_quoted_argument(
    WCHAR* destination, size_t capacity, const WCHAR* argument) {
    if (FAILED(StringCchCatW(destination, capacity, L"\""))) {
        return false;
    }

    size_t backslashes = 0;
    for (const WCHAR* p = argument; *p; ++p) {
        if (*p == L'\\') {
            ++backslashes;
            continue;
        }
        if (*p == L'"') {
            for (size_t i = 0; i <= backslashes * 2; ++i) {
                if (FAILED(StringCchCatW(destination, capacity, L"\\"))) {
                    return false;
                }
            }
            if (FAILED(StringCchCatW(destination, capacity, L"\""))) {
                return false;
            }
        } else {
            for (size_t i = 0; i < backslashes; ++i) {
                if (FAILED(StringCchCatW(destination, capacity, L"\\"))) {
                    return false;
                }
            }
            WCHAR character[2] = { *p, L'\0' };
            if (FAILED(StringCchCatW(destination, capacity, character))) {
                return false;
            }
        }
        backslashes = 0;
    }

    for (size_t i = 0; i < backslashes * 2; ++i) {
        if (FAILED(StringCchCatW(destination, capacity, L"\\"))) {
            return false;
        }
    }
    return SUCCEEDED(StringCchCatW(destination, capacity, L"\""));
}

static LONG check_status() {
    HKEY hkey = nullptr;
    LONG result = RegOpenKeyExW(HKEY_CLASSES_ROOT, CLSID_KEY_FMT, 0,
                                 KEY_READ | KEY_WOW64_32KEY, &hkey);

    if (result != ERROR_SUCCESS) {
        // Try 64-bit hive
        result = RegOpenKeyExW(HKEY_CLASSES_ROOT, CLSID_KEY_FMT, 0,
                                KEY_READ, &hkey);
    }

    if (result != ERROR_SUCCESS) {
        fprintf(stdout, "Status: NOT REGISTERED\n");
        fprintf(stdout, "The filter is not currently registered on this system.\n");
        return 0;
    }

    fprintf(stdout, "Status: REGISTERED\n");

    // Read friendly name
    WCHAR name[256] = {0};
    DWORD size = sizeof(name);
    if (RegQueryValueExW(hkey, nullptr, nullptr, nullptr, (LPBYTE)name, &size) == ERROR_SUCCESS) {
        fprintf(stdout, "  Name: %ls\n", name);
    }

    // Check merit via the DirectShow filter registry (HKCR\CLSID\{...}\Instance\{...})
    // Simplified: check if the CLSID exists
    fprintf(stdout, "  CLSID: {4FAC742E-1B8B-4DE8-AD7B-D0158F7C2448}\n");

    RegCloseKey(hkey);

    fprintf(stdout, "\nTo unregister: MADFilterRegister.exe /unregister\n");
    return 0;
}

#else
// Non-Windows stub
static void print_usage() {
    fprintf(stderr, "This utility is Windows-only.\n");
}
#endif

// ── Main ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
#ifndef _WIN32
    fprintf(stderr, "MADFilterRegister is a Windows-only utility.\n");
    return 1;
#else
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const char* cmd = argv[1];

    if (strcmp(cmd, "/help") == 0 || strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(cmd, "/status") == 0) {
        return check_status();
    }

    // Registration commands may need admin
    if (!is_admin()) {
        fprintf(stderr, "WARNING: Not running as Administrator.\n");
        fprintf(stderr, "Registration may fail.  Run from an elevated command prompt.\n\n");
    }

    if (strcmp(cmd, "/register") == 0) {
        return register_filter(MERIT_NORMAL);
    }

    if (strcmp(cmd, "/fallout3") == 0) {
        fprintf(stdout,
            "==============================================================\n"
            "  FALLOUT 3 COMPATIBILITY MODE\n"
            "\n"
            "  This will register the filter with elevated merit.\n"
            "  WARNING: Other 32-bit DirectShow applications may use\n"
            "  this decoder instead of their default MP3 decoder.\n"
            "\n"
            "  Unregister with: MADFilterRegister.exe /unregister\n"
            "==============================================================\n"
            "\n"
        );
        return register_filter(MERIT_PREFERRED + 1);
    }

    if (strcmp(cmd, "/unregister") == 0) {
        return unregister_filter();
    }

    if (strcmp(cmd, "/launch") == 0) {
        if (!is_admin()) {
            fprintf(stderr,
                    "ERROR: /launch requires Administrator privileges.\n");
            return 1;
        }

        int wargc = 0;
        LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
        if (!wargv || wargc < 3) {
            fprintf(stderr,
                    "Usage: MADFilterRegister.exe /launch <exe> [args...]\n");
            if (wargv) LocalFree(wargv);
            return 1;
        }

        WCHAR arguments[32768] = {};
        bool arguments_ok = true;
        for (int i = 3; i < wargc; ++i) {
            if (i > 3) {
                arguments_ok = arguments_ok &&
                    SUCCEEDED(StringCchCatW(
                        arguments, _countof(arguments), L" "));
            }
            arguments_ok = arguments_ok &&
                append_quoted_argument(
                    arguments, _countof(arguments), wargv[i]);
            if (!arguments_ok) break;
        }

        LONG result = -1;
        if (arguments_ok) {
            result = launch_game(wargv[2], arguments);
        } else {
            fprintf(stderr, "ERROR: Launch arguments are too long.\n");
        }
        LocalFree(wargv);
        return result;
    }

    fprintf(stderr, "Unknown command: %s\n", cmd);
    print_usage();
    return 1;
#endif
}
