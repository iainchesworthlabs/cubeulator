# SPDX-FileCopyrightText: 2026 Iain Chesworth
# SPDX-License-Identifier: GPL-3.0-or-later
# CMake 4 is required; the version range keeps policy behaviour current up to 4.4.
cmake_minimum_required(VERSION 3.28...4.4)

# ---------------------------------------------------------------------------
# vcpkg (pinned by the builtin-baseline in vcpkg.json).
#
# The baseline names a commit of the labs' vcpkg fork. Which checkout of that
# commit a configure uses - one shared between your checkouts through
# ICL_VCPKG_STORE, a root you name, or the VCPKG_ROOT that CI sets - is decided
# in cmake/icl/IclVcpkgRoot.cmake (docs/building.md), which sets
# CMAKE_TOOLCHAIN_FILE. The presets deliberately do not. The module is included
# by path: cmake/icl is not on CMAKE_MODULE_PATH, so its FindQt6 does not take
# the place of cmake/FindQt6.cmake.
# ---------------------------------------------------------------------------
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/icl/IclVcpkgRoot.cmake")
icl_resolve_vcpkg("${CMAKE_CURRENT_SOURCE_DIR}" CUBEULATOR)

# ---------------------------------------------------------------------------
# Select vcpkg manifest features BEFORE project() (the vcpkg toolchain reads
# VCPKG_MANIFEST_FEATURES when it is loaded at project() time). Qt is a
# prebuilt dependency found via CMAKE_PREFIX_PATH (see ci/), not vcpkg.
# ---------------------------------------------------------------------------
if(NOT DEFINED CUBEULATOR_BUILD_TESTS OR CUBEULATOR_BUILD_TESTS)
    list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()

# Tracy is built into the desktop applications (Windows, Linux, macOS) and
# dormant in them: the one switch CUBEULATOR_ENABLE_TRACY (declared in
# cmake/Tracy.cmake) both makes the profiler client resolvable and turns the
# instrumentation macros real, and cube::profile::Session decides at run time
# whether the client is started (docs/profiling.md). The default has to be
# settled here, before project(), because it selects a vcpkg manifest feature -
# and the platform is not known until Platform.cmake, after project(), so which
# platforms have no Tracy is the presets' say, not this file's: the web,
# Android and iOS presets set CUBEULATOR_ENABLE_TRACY=OFF (the browser has the
# in-process recorder instead). It defaults off for a coverage build, whose
# timings are not about the program; configure Windows on ARM (the port does
# not support it) with -DCUBEULATOR_ENABLE_TRACY=OFF.
if(NOT DEFINED CUBEULATOR_ENABLE_TRACY)
    if(CUBEULATOR_ENABLE_PROFILE_RECORDER OR CUBEULATOR_ENABLE_COVERAGE)
        set(_cubeulator_tracy_default OFF)
    else()
        set(_cubeulator_tracy_default ON)
    endif()
    set(CUBEULATOR_ENABLE_TRACY ${_cubeulator_tracy_default} CACHE BOOL
        "Build the Tracy profiler client into the application (started at run time, see docs/profiling.md)")
endif()
option(CUBEULATOR_TRACY_TOOLS
    "Also build Tracy's command-line tools (tracy-capture, tracy-csvexport) for the host" OFF)
if(CUBEULATOR_ENABLE_TRACY)
    list(APPEND VCPKG_MANIFEST_FEATURES "profiling")
    if(CUBEULATOR_TRACY_TOOLS)
        list(APPEND VCPKG_MANIFEST_FEATURES "profiling-tools")
    endif()
    # The tracy port is an overlay (vcpkg-overlay-ports/tracy): the registry's has no
    # feature for manual lifetime or loopback-only listening.
    list(APPEND VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg-overlay-ports")
endif()

project(Cubeulator
    VERSION 0.2.0
    DESCRIPTION "A camera-driven, on-device Rubik's Cube solver"
    HOMEPAGE_URL "https://github.com/iainchesworthlabs/cubeulator"
    LANGUAGES CXX)

# The pre-release label of this version: "beta.1" for 0.1.0-beta.1, "" for a
# final release. project(VERSION) cannot hold a label, so it sits beside it.
# tools/release/prepare_release.py sets the pair together, and the Release
# workflow checks the tag against it (docs/releasing.md). Everything that is
# named for the version - the packages, the version resource, version.hpp's
# version_string - uses CUBEULATOR_VERSION_FULL, so a beta is never a package
# called by the final's name.
set(CUBEULATOR_VERSION_PRERELEASE "beta.3")
set(CUBEULATOR_VERSION_FULL "${PROJECT_VERSION}")
if(NOT CUBEULATOR_VERSION_PRERELEASE STREQUAL "")
    if(NOT CUBEULATOR_VERSION_PRERELEASE MATCHES "^(alpha|beta|rc)\\.[1-9][0-9]*$")
        message(FATAL_ERROR
            "CUBEULATOR_VERSION_PRERELEASE is '${CUBEULATOR_VERSION_PRERELEASE}'; "
            "expected alpha.N, beta.N or rc.N, or empty for a final release.")
    endif()
    string(APPEND CUBEULATOR_VERSION_FULL "-${CUBEULATOR_VERSION_PRERELEASE}")
endif()

# ---------------------------------------------------------------------------
# Global language settings: C++23 only, no compiler extensions.
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for clang-tidy / editors.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Put all runtime artefacts in one place so the Qt app can find its plugins.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
# The one place the target OS is decided (cmake/Platform.cmake); every other
# CMake file reads what the platform's fragment declared.
include(Platform)
include(CompilerWarnings)
include(Hardening)
include(Sanitizers)
include(Coverage)
include(Tracy)
include(ProfileRecorder)
include(Simd)

# Which variants/simd-<name> cube::lib is built with (cmake/Simd.cmake).
cube_resolve_simd(CUBEULATOR_SIMD_RESOLVED)
include(MobileDependencies)

cube_configure_mobile_dependencies()

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(CUBEULATOR_BUILD_APP "Build the Qt6 GUI application" ON)
option(CUBEULATOR_BUILD_TESTS "Build the unit and integration test suites" ON)
# The fuzz harnesses of src/lib/fuzz. Their seeds and regressions are replayed by
# ctest (label `fuzz`), so they are built wherever the tests are; the
# linux-clang-fuzz preset builds them without the tests and with libFuzzer.
option(CUBEULATOR_BUILD_FUZZERS "Build the fuzz harnesses (src/lib/fuzz)" ${CUBEULATOR_BUILD_TESTS})

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------
# The library holds ALL of the solver/vision/render/i18n logic and is
# completely GUI-free.
add_subdirectory(src/lib)

# Always added: src/app/CMakeLists.txt builds the platform-abstraction
# library (cube_platform) unconditionally so the per-OS interface selection
# is exercised on every preset, including CUBEULATOR_BUILD_APP=OFF headless
# builds; the Qt GUI executable inside it is separately gated on
# CUBEULATOR_BUILD_APP.
add_subdirectory(src/app)

# Desktop-only developer instruments (the scan scoreboard - see
# docs/recovery-plan.md); gated per-platform inside.
add_subdirectory(tools)

if(CUBEULATOR_BUILD_TESTS OR CUBEULATOR_BUILD_FUZZERS)
    enable_testing()
endif()

if(CUBEULATOR_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(CUBEULATOR_BUILD_FUZZERS)
    add_subdirectory(src/lib/fuzz)
endif()

include(Packaging)

message(STATUS "Cubeulator ${CUBEULATOR_VERSION_FULL}")
message(STATUS "  Build app   : ${CUBEULATOR_BUILD_APP}")
message(STATUS "  Build tests : ${CUBEULATOR_BUILD_TESTS}")
message(STATUS "  Fuzzers     : ${CUBEULATOR_BUILD_FUZZERS}")
message(STATUS "  Tracy       : ${CUBEULATOR_ENABLE_TRACY}")
message(STATUS "  SIMD kernels: ${CUBEULATOR_SIMD_RESOLVED}")
message(STATUS "  Recorder    : ${CUBEULATOR_ENABLE_PROFILE_RECORDER}")
