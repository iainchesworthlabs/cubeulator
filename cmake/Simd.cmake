# ---------------------------------------------------------------------------
# Simd.cmake
#
# CUBEULATOR_SIMD chooses the implementation of the per-pixel channel kernels
# (cube/vision/detail/channel_kernels.hpp) that cube::lib is built with, in
# place of an #ifdef in the source: src/lib/CMakeLists.txt puts one of
# src/lib/variants/simd-<name> on cube::lib's include path, the same mechanism
# as the profiling variants (cmake/Tracy.cmake) and the one iclforge's arch
# tree uses.
#
#   auto       (default) vector128 where the target has a 128-bit vector unit
#              the architecture guarantees - x86-64 (SSE2), arm64 (NEON) - or
#              the platform's fragment says its flags give one
#              (CUBEULATOR_PLATFORM_SIMD, the browser's -msimd128); generic
#              everywhere else (32-bit x86, 32-bit ARM, anything unrecognised).
#   generic    the scalar loops only. The reference a reproducibility
#              comparison should reach for first, and what a run with
#              cv::setUseOptimized(false) falls back to inside vector128.
#   vector128  the OpenCV universal intrinsics at 128 bits. Refused on a target
#              the check below does not recognise rather than quietly built
#              with OpenCV's much slower C++ stand-in for a missing unit.
#
# There is no wider tier (AVX2, AVX-512) and so no CPU probe: measured, the
# 128-bit kernel reads a 640 x 360 frame in 0.045 ms against a memory floor of
# about 0.02 ms (docs/performance.md). What can differ by CPU, and what OpenCV
# dispatches at run time (its own AVX2 kernels, behind cv::useOptimized()), is
# in OpenCV's functions the reader calls; the kernels here ask the same switch.
#
# The architecture is the effective one of the compile line, not the host's: on
# Apple, CMAKE_OSX_ARCHITECTURES overrides CMAKE_SYSTEM_PROCESSOR per compile
# line, so it is read first, and a universal binary is vector128 when every
# slice is x86_64 or arm64 (OpenCV's intrinsics follow each slice's own flags).
# ---------------------------------------------------------------------------

set(CUBEULATOR_SIMD "auto" CACHE STRING
    "128-bit vector kernels: auto (from the target architecture), generic or vector128")
set_property(CACHE CUBEULATOR_SIMD PROPERTY STRINGS auto generic vector128)

function(cube_resolve_simd result)
    set(_requested "${CUBEULATOR_SIMD}")
    if(NOT _requested MATCHES "^(auto|generic|vector128)$")
        message(FATAL_ERROR
            "CUBEULATOR_SIMD='${_requested}' is not one of auto, generic, vector128.")
    endif()

    # The effective architectures of the compile line.
    if(CMAKE_OSX_ARCHITECTURES)
        set(_architectures ${CMAKE_OSX_ARCHITECTURES})
    else()
        set(_architectures ${CMAKE_SYSTEM_PROCESSOR})
    endif()
    string(TOLOWER "${_architectures}" _architectures)
    set(_vector_architectures x86_64 amd64 arm64 aarch64 arm64e)
    set(_recognised TRUE)
    if(NOT _architectures)
        set(_recognised FALSE)
    endif()
    foreach(_architecture IN LISTS _architectures)
        if(NOT _architecture IN_LIST _vector_architectures)
            set(_recognised FALSE)
        endif()
    endforeach()
    if(CUBEULATOR_PLATFORM_SIMD STREQUAL "vector128")
        set(_recognised TRUE)
    endif()

    if(_requested STREQUAL "auto")
        set(_resolved generic)
        if(_recognised)
            set(_resolved vector128)
        endif()
    elseif(_requested STREQUAL "vector128" AND NOT _recognised)
        message(FATAL_ERROR
            "CUBEULATOR_SIMD=vector128, but '${_architectures}' is not a target "
            "with a 128-bit vector unit this build can count on (x86-64, arm64, "
            "or a platform whose fragment sets CUBEULATOR_PLATFORM_SIMD). Use "
            "auto or generic.")
    else()
        set(_resolved "${_requested}")
    endif()

    if(NOT EXISTS "${PROJECT_SOURCE_DIR}/src/lib/variants/simd-${_resolved}")
        message(FATAL_ERROR
            "CUBEULATOR_SIMD resolved to '${_resolved}', which has no "
            "src/lib/variants/simd-${_resolved}/cube/vision/detail/channel_kernels.hpp.")
    endif()
    set(${result} "${_resolved}" PARENT_SCOPE)
endfunction()
