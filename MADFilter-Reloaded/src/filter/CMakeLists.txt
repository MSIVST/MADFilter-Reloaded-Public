# src/filter/CMakeLists.txt — DirectShow filter DLL

set(FILTER_SOURCES
    MADFilterReloaded.cpp
    MADFilterReloaded.h
    maddecode.cpp
    maddecode.h
    CLSIDs.h
    MADFilterReloaded.def
    MADFilterReloaded.rc
)

# ── Find DirectShow base classes ──────────────────────────────────────
# The DirectShow base classes (strmbase.lib) are part of the Windows SDK.
# We look for them in the SDK include path.

# Common locations for the DirectShow base class headers:
find_path(DIRECTSHOW_INCLUDE_DIR
    NAMES streams.h
    PATHS
        "$ENV{WindowsSdkDir}Include/$ENV{WindowsSDKVersion}/um"
        "$ENV{WindowsSdkDir}Include/*/um"
        "C:/Program Files (x86)/Windows Kits/10/Include/*/um"
        "C:/Program Files/Microsoft SDKs/Windows/*/Include"
    PATH_SUFFIXES um
)

# The DirectShow base class library might be in the SDK lib path
find_library(STRMBASE_LIB
    NAMES strmbase strmbasd
    PATHS
        "$ENV{WindowsSdkDir}Lib/$ENV{WindowsSDKVersion}/um/x86"
        "$ENV{WindowsSdkDir}Lib/*/um/x86"
        "C:/Program Files (x86)/Windows Kits/10/Lib/*/um/x86"
)

if(NOT DIRECTSHOW_INCLUDE_DIR)
    message(FATAL_ERROR
        "DirectShow base-class header streams.h was not found. "
        "Install/build the Windows SDK DirectShow base-class samples, then "
        "set DIRECTSHOW_INCLUDE_DIR to the directory containing streams.h.")
endif()

if(NOT STRMBASE_LIB)
    message(FATAL_ERROR
        "DirectShow base-class library strmbase.lib was not found. "
        "Set STRMBASE_LIB to the x86 library path.")
endif()

# ── Filter DLL target ─────────────────────────────────────────────────
add_library(MADFilterReloaded SHARED ${FILTER_SOURCES})

target_include_directories(MADFilterReloaded PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/filter"
    "${CMAKE_SOURCE_DIR}/src/common"
    "${CMAKE_BINARY_DIR}/libmad"     # For generated mad.h
    "${DIRECTSHOW_INCLUDE_DIR}"
)

target_link_libraries(MADFilterReloaded PRIVATE
    madfilter-common
    ${STRMBASE_LIB}
)

# Standard Windows libraries required by DirectShow
target_link_libraries(MADFilterReloaded PRIVATE
    strmiids     # DirectShow CLSID/IID definitions
    quartz       # DirectShow runtime
    winmm        # Multimedia (timeGetTime, etc.)
    ole32        # COM
    oleaut32     # COM automation
    uuid         # UUID support
)

# ── Compiler settings ─────────────────────────────────────────────────
target_compile_definitions(MADFilterReloaded PRIVATE
    _USRDLL
    MADFILTER_EXPORTS
    WIN32_LEAN_AND_MEAN
)

# ── Linker settings ───────────────────────────────────────────────────
set_target_properties(MADFilterReloaded PROPERTIES
    OUTPUT_NAME "MADFilterReloaded"
    SUFFIX ".ax"
)

madfilter_harden_target(MADFilterReloaded)

if(MSVC)
    # The Windows SDK strmbase.lib ABI uses WCHAR as unsigned short and
    # references pre-UCRT stdio entry points.
    target_compile_options(MADFilterReloaded PRIVATE /Zc:wchar_t-)
    target_link_libraries(MADFilterReloaded PRIVATE
        legacy_stdio_definitions
    )
endif()

set_target_properties(MADFilterReloaded PROPERTIES FOLDER "src")
