cmake_minimum_required(VERSION 3.16)
cmake_policy(SET CMP0091 NEW)
project(MADFilter-Reloaded VERSION 2.1.0.0 LANGUAGES C CXX)

# ── Target architecture ───────────────────────────────────────────────
# This project targets 32-bit x86 for Fallout 3 compatibility.
# Build with: cmake -A Win32 ...
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ── Options ───────────────────────────────────────────────────────────
option(BUILD_TESTS "Build test harness" ON)
option(BUILD_FUZZER "Build fuzzing harness" OFF)
option(BUILD_LIBFUZZER "Build coverage-guided libFuzzer target (clang-cl)" OFF)
option(BUILD_FILTER "Build the x86 DirectShow filter and registration utility" ON)
option(ENABLE_ASAN "Enable AddressSanitizer (MSVC)" OFF)
option(TARGET_WINDOWS_XP "Target the single Windows XP-through-11 release" ON)

# All distributable Windows filter builds use the same XP-compatible toolchain.
# A modern compiler is allowed only for internal AddressSanitizer diagnostics.
if(MSVC AND BUILD_FILTER AND NOT ENABLE_ASAN)
    if(NOT TARGET_WINDOWS_XP OR NOT CMAKE_VS_PLATFORM_TOOLSET STREQUAL "v141_xp")
        message(FATAL_ERROR
            "Release filter builds require -A Win32 -T v141_xp and "
            "TARGET_WINDOWS_XP=ON. There is one XP-through-11 release build. "
            "Modern toolchains are reserved for ENABLE_ASAN diagnostics.")
    endif()
endif()

if(TARGET_WINDOWS_XP)
    set(MADFILTER_WINVER 0x0501)
else()
    set(MADFILTER_WINVER 0x0600)
endif()

# Release binaries are self-contained so old systems do not need a current
# Visual C++ Redistributable. ASan requires the dynamic runtime.
if(MSVC)
    if(ENABLE_ASAN)
        set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY
            "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

# ── Compiler settings ─────────────────────────────────────────────────
# Keep first-party warning and hardening policy off third-party libmad.
function(madfilter_harden_target target)
    if(MSVC)
        target_compile_definitions(${target} PRIVATE
            _WIN32_WINNT=${MADFILTER_WINVER}
            WINVER=${MADFILTER_WINVER}
        )
        target_compile_options(${target} PRIVATE /W4 /WX /GS /sdl /guard:cf)
        target_link_options(${target} PRIVATE
            /NXCOMPAT /DYNAMICBASE /guard:cf)

        if(ENABLE_ASAN)
            target_compile_options(${target} PRIVATE /fsanitize=address /Zi)
            target_link_options(${target} PRIVATE
                /INCREMENTAL:NO /DEBUG)
        endif()

        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            target_link_options(${target} PRIVATE /SAFESEH)
        endif()
    endif()
endfunction()

# ── Third-party: libmad ───────────────────────────────────────────────
# Use the Tenacity fork of libmad
set(LIBMAD_DIR "${CMAKE_SOURCE_DIR}/../libmad")

# Build libmad as a static library
add_subdirectory("${LIBMAD_DIR}" "${CMAKE_BINARY_DIR}/libmad")
set_target_properties(mad-static PROPERTIES FOLDER "third_party")
if(MSVC)
    # libmad parses untrusted input and is linked into the filter, so retain
    # exploit mitigations even though third-party warnings remain untouched.
    target_compile_options(mad-static PRIVATE /GS /guard:cf)
endif()

# ── Common library ────────────────────────────────────────────────────
add_library(madfilter-common INTERFACE)
target_sources(madfilter-common INTERFACE
    src/common/safe_math.h
    src/common/bounded_buffer.h
)
target_include_directories(madfilter-common INTERFACE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/common"
)
target_link_libraries(madfilter-common INTERFACE mad-static)
set_target_properties(madfilter-common PROPERTIES FOLDER "src")

# ── Filter DLL ────────────────────────────────────────────────────────
if(BUILD_FILTER)
    if(NOT WIN32)
        message(FATAL_ERROR "BUILD_FILTER requires Windows")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "BUILD_FILTER requires a 32-bit toolchain. "
            "Use -A Win32 with Visual Studio, or set -DBUILD_FILTER=OFF "
            "for the portable CLI tools.")
    endif()

    add_subdirectory(src/filter)

    add_executable(MADFilterRegister
        src/tools/MADFilterRegister.cpp
        src/tools/MADFilterRegister.rc
    )
    target_link_libraries(MADFilterRegister PRIVATE
        advapi32
        ole32
        shell32
    )
    madfilter_harden_target(MADFilterRegister)
    set_target_properties(MADFilterRegister PROPERTIES FOLDER "tools")
endif()

# ── Tests ─────────────────────────────────────────────────────────────
if(BUILD_TESTS)
    enable_testing()

    add_executable(boundary_test
        tests/unit/boundary_test.cpp
        src/filter/maddecode.cpp
    )
    target_link_libraries(boundary_test PRIVATE madfilter-common)
    madfilter_harden_target(boundary_test)
    add_test(NAME boundary_test COMMAND boundary_test)

    add_executable(maddec_test
        tests/unit/maddec_test.cpp
        src/filter/maddecode.cpp
    )
    if(MSVC)
        target_compile_definitions(maddec_test PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_link_libraries(maddec_test PRIVATE madfilter-common)
    madfilter_harden_target(maddec_test)

    if(BUILD_FILTER)
        add_executable(register_test tests/unit/register_test.cpp)
        target_link_libraries(register_test PRIVATE advapi32 ole32 shell32)
        madfilter_harden_target(register_test)
        add_test(NAME register_test COMMAND register_test)

        add_executable(filter_load_test
            tests/unit/filter_load_test.cpp
        )
        target_link_libraries(filter_load_test PRIVATE ole32)
        madfilter_harden_target(filter_load_test)
        add_test(
            NAME filter_load_test
            COMMAND filter_load_test "$<TARGET_FILE:MADFilterReloaded>"
        )

        add_executable(filter_sample_test
            tests/unit/filter_sample_test.cpp
            src/filter/MADFilterReloaded.cpp
            src/filter/maddecode.cpp
        )
        target_include_directories(filter_sample_test PRIVATE
            "${CMAKE_SOURCE_DIR}/src"
            "${CMAKE_SOURCE_DIR}/src/filter"
            "${CMAKE_SOURCE_DIR}/src/common"
            "${CMAKE_BINARY_DIR}/libmad"
            "${DIRECTSHOW_INCLUDE_DIR}"
        )
        target_compile_definitions(filter_sample_test PRIVATE
            WIN32_LEAN_AND_MEAN
        )
        target_link_libraries(filter_sample_test PRIVATE
            madfilter-common
            ${STRMBASE_LIB}
            strmiids
            quartz
            winmm
            ole32
            oleaut32
            uuid
        )
        if(MSVC)
            target_compile_options(filter_sample_test PRIVATE /Zc:wchar_t-)
            target_link_libraries(filter_sample_test PRIVATE legacy_stdio_definitions)
        endif()
        madfilter_harden_target(filter_sample_test)
        add_test(NAME filter_sample_test COMMAND filter_sample_test)

        add_executable(filter_graph_test
            tests/unit/filter_graph_test.cpp
        )
        target_link_libraries(filter_graph_test PRIVATE
            strmiids
            quartz
            ole32
            oleaut32
        )
        madfilter_harden_target(filter_graph_test)
    endif()
endif()

if(BUILD_FUZZER)
    add_executable(madfuzz
        tests/unit/madfuzz.cpp
        src/filter/maddecode.cpp
    )
    if(MSVC)
        target_compile_definitions(madfuzz PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
    target_link_libraries(madfuzz PRIVATE madfilter-common)
    madfilter_harden_target(madfuzz)
endif()

if(BUILD_LIBFUZZER)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "BUILD_LIBFUZZER requires clang-cl")
    endif()
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "BUILD_LIBFUZZER requires an x86 toolchain")
    endif()

    add_executable(madfuzz_libfuzzer
        tests/unit/madfuzz.cpp
        src/filter/maddecode.cpp
    )
    target_compile_definitions(madfuzz_libfuzzer PRIVATE
        MADFILTER_LIBFUZZER
        _CRT_SECURE_NO_WARNINGS
    )
    target_compile_options(madfuzz_libfuzzer PRIVATE
        -fsanitize=fuzzer,address
        -fno-omit-frame-pointer
        /W4
        /WX
    )
    target_link_libraries(madfuzz_libfuzzer PRIVATE
        madfilter-common
    )
    target_link_options(madfuzz_libfuzzer PRIVATE
        -fsanitize=fuzzer,address
    )
endif()
