cmake_minimum_required(VERSION 3.10)
project(mad VERSION 0.16.4)

# The library SOVERSION. This is set to 0 for backward compatibility.
# The general policy is that minor versions of the library (e.g., 0.16.1,
# 0.16.2) don't constitute a major ABI breakage. Major versions (e.g., 0.17,
# 0.18) do.
set(LIBRARY_SOVERSION 0)

include(GNUInstallDirs)
include(CheckTypeSize)

#
# Build
#

set(MAD_SOURCES
    src/bit.c
    src/decoder.c
    src/fixed.c
    src/frame.c
    src/huffman.c
    src/layer12.c
    src/layer3.c
    src/stream.c
    src/synth.c
    src/timer.c
    src/version.c
)

# Define both targets explicitly
add_library(mad SHARED ${MAD_SOURCES})
add_library(mad-static STATIC ${MAD_SOURCES})

# Ensure the static library is named libmad.a
set_target_properties(mad-static PROPERTIES OUTPUT_NAME mad)

set(ALL_MAD_TARGETS mad mad-static)

foreach(t ${ALL_MAD_TARGETS})
    target_include_directories(${t} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
endforeach()

set_target_properties(mad PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${LIBRARY_SOVERSION}
)

#
# Compile definitions
#

option(OPTIMIZE "Optimize for SPEED (default) or ACCURACY" SPEED)
if(OPTIMIZE STREQUAL "SPEED")
  message(STATUS "Optimizing for speed over accuracy.")
  set(OPT_DEF OPT_SPEED)
else()
  message(STATUS "Optimizing for accuracy over speed.")
  set(OPT_DEF OPT_ACCURACY)
endif()

option(ASO "Enable CPU Architecture Specific Optimizations (x86, ARM, and MIPS only)" ON)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(STATUS "Using 64 bit fixed point math")
  option(FPM_64BIT "64 bit fixed point math" ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(x|i[3456])86" OR
       CMAKE_C_COMPILER_ARCHITECTURE_ID STREQUAL "X86" OR
       (MSVC AND CMAKE_GENERATOR_PLATFORM STREQUAL "Win32"))
  message(STATUS "Using x86 fixed point math")
  option (FPM_INTEL "x86 fixed point math" ON)
  if(ASO)
    list(APPEND MAD_CONFIG_DEFS ASO_ZEROCHECK)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*(arm|ARM).*")
  message(STATUS "Using ARM fixed point math")
  option (FPM_ARM "ARM fixed point math" ON)
  if(ASO)
    enable_language(ASM)
    list(APPEND MAD_CONFIG_DEFS ASO_INTERLEAVE1 ASO_IMDCT)
    target_sources(mad PRIVATE src/imdct_l_arm.S)
    target_sources(mad-static PRIVATE src/imdct_l_arm.S)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*(mips|MIPS).*")
  message(STATUS "Using MIPS fixed point math")
  option(FPM_MIPS "MIPS fixed point math" ON)
  if(ASO)
    list(APPEND MAD_CONFIG_DEFS ASO_INTERLEAVE2 ASO_ZEROCHECK)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".*(sparc|SPARC).*")
  message(STATUS "Using SPARC fixed point math")
  option(FPM_SPARC "SPARC fixed point math" ON)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES ".(ppc|PPC|powerpc).*")
  message(STATUS "Using PowerPC fixed point math")
  option(FPM_PPC "PowerPC fixed point math" ON)
else()
  message(WARNING "Target CPU architecture not detected. Fixed-point math will yield limited accuracy.")
  option(FPM_DEFAULT "Generic fixed-point math" ON)
endif()

option(PAD_VERSION_STRING "Pads the version string for improved 0.15.1b ABI compatibility" OFF)
if (PAD_VERSION_STRING)
  message(STATUS "Version string padding enabled")
  set(MAD_VERSION_EXTRA "\"       \"")
else()
  set(MAD_VERSION_EXTRA "")
endif()

# Initialize definition list with optimization choice
list(APPEND MAD_CONFIG_DEFS ${OPT_DEF})

check_type_size(int SIZEOF_INT BUILTIN_TYPES_ONLY LANGUAGE C)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/mad.h.in ${CMAKE_CURRENT_BINARY_DIR}/mad.h @ONLY)

include(CheckIncludeFile)
macro(mad_check_include file var)
  check_include_file(${file} ${var})
  if(${var})
    list(APPEND MAD_CONFIG_DEFS ${var})
  endif()
endmacro()

mad_check_include(sys/types.h HAVE_SYS_TYPES_H)
mad_check_include(sys/wait.h HAVE_SYS_WAIT_H)
mad_check_include(sys/mman.h HAVE_SYS_MMAN_H)
mad_check_include(sys/stat.h HAVE_SYS_STAT_H)
mad_check_include(unistd.h HAVE_UNISTD_H)
mad_check_include(assert.h HAVE_ASSERT_H)
mad_check_include(fcntl.h HAVE_FCNTL_H)
mad_check_include(limits.h HAVE_LIMITS_H)

include(CheckFunctionExists)
macro(mad_check_function func var)
  check_function_exists(${func} ${var})
  if(${var})
    list(APPEND MAD_CONFIG_DEFS ${var})
  endif()
endmacro()

mad_check_function(ftruncate HAVE_FTRUNCATE)
mad_check_function(pipe HAVE_PIPE)
mad_check_function(fork HAVE_FORK)
mad_check_function(waitpid HAVE_WAITPID)

# Apply all collected definitions to both targets
foreach(t ${ALL_MAD_TARGETS})
  target_compile_definitions(${t} PRIVATE ${MAD_CONFIG_DEFS})
endforeach()

#
# Example application
#

include(CMakeDependentOption)
cmake_dependent_option(EXAMPLE "Build example executable" ON "HAVE_UNISTD_H;HAVE_SYS_STAT_H;HAVE_SYS_MMAN_H" OFF)
if(EXAMPLE)
  add_executable(mad_example examples/minimad.c)
  target_link_libraries(mad_example PRIVATE mad)
endif()

#
# Installation
#

include(CMakePackageConfigHelpers)

# Library files
install(TARGETS mad mad-static
  EXPORT madTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Header files
install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/mad.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# pkgconfig
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  set(PKGCONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
  set(PKGCONFIG_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
  set(PKGCONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(PKGCONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/packaging/mad.pc.in
  ${CMAKE_CURRENT_BINARY_DIR}/packaging/mad.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/packaging/mad.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# CMake config
set(MAD_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/mad")
install(
  EXPORT madTargets
  FILE madTargets.cmake
  NAMESPACE mad::
  DESTINATION "${MAD_INSTALL_CMAKEDIR}"
)
configure_package_config_file(packaging/madConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/packaging/madConfig.cmake"
  INSTALL_DESTINATION "${MAD_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/packaging/madConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/packaging/madConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/packaging/madConfigVersion.cmake"
  DESTINATION "${MAD_INSTALL_CMAKEDIR}"
)

# CPack setup
list(
  APPEND CPACK_SOURCE_IGNORE_FILES
    "${CMAKE_SOURCE_DIRECTORY}/build.*"
	  "${CMAKE_SOURCE_DIRECTORY}/\\.git/"
)

set(CPACK_SOURCE_PACKAGE_FILE_NAME "mad-${PROJECT_VERSION}-source")
set(CPACK_PACKAGE_CHECKSUM "SHA256")

include(CPack)
