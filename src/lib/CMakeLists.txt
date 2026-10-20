# ---------------------------------------------------------------------------
# cube::lib - solver, vision and i18n, no GUI, no platform code.
# ---------------------------------------------------------------------------
add_library(cube_lib)
add_library(cube::lib ALIAS cube_lib)

# fmt fills the gap std::format leaves on Android: the NDK's libc++ doesn't
# implement <format> (it needs locale support the NDK's minimal libc doesn't
# provide). Resolved via vcpkg on every triplet, including mobile - unlike
# opencv4, fmt has no native/platform-specific code to make
# mobile triplets risky (see docs/dependency-strategy.md).
find_package(fmt CONFIG REQUIRED)

# --- Version + git stamp ---------------------------------------------------
find_package(Git QUIET)
set(CUBEULATOR_VERSION_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/include/cube/version.hpp.in")
set(CUBEULATOR_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/cube/version.hpp")

# What this build targets ("Windows x86_64 (MSVC 1951)"), worked out from
# the platform fragment and the toolchain here rather than by predefined
# macros inside a source file - this library holds no platform code.
cube_describe_build_target(CUBEULATOR_BUILD_TARGET)

# Generate once at configure time so the header exists for the first build.
set(CUBEULATOR_VERSION "${CUBEULATOR_VERSION_FULL}")
set(SRC "${CUBEULATOR_VERSION_TEMPLATE}")
set(DST "${CUBEULATOR_VERSION_HEADER}")
set(WORKDIR "${PROJECT_SOURCE_DIR}")
include("${PROJECT_SOURCE_DIR}/cmake/GenerateVersion.cmake")

# Re-stamp on every build so the git information tracks new commits without a
# reconfigure. configure_file only rewrites when the content changes, so this
# does not needlessly recompile dependants.
add_custom_target(cube_version ALL
    BYPRODUCTS "${CUBEULATOR_VERSION_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
        "-DCUBEULATOR_VERSION=${CUBEULATOR_VERSION_FULL}"
        "-DCUBEULATOR_BUILD_TARGET=${CUBEULATOR_BUILD_TARGET}"
        "-DSRC=${CUBEULATOR_VERSION_TEMPLATE}"
        "-DDST=${CUBEULATOR_VERSION_HEADER}"
        "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
        "-DWORKDIR=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/GenerateVersion.cmake"
    COMMENT "Stamping version and git information"
    VERBATIM)

target_sources(cube_lib
    PRIVATE
        src/version.cpp
        src/solver/cube.cpp
        src/solver/scan_assembly.cpp
        src/solver/notation.cpp
        src/solver/outside_view.cpp
        src/solver/hold.cpp
        src/solver/cross.cpp
        src/solver/corners.cpp
        src/solver/middle_edges.cpp
        src/solver/oll.cpp
        src/solver/pll.cpp
        src/solver/cubie.cpp
        src/solver/two_phase.cpp
        src/solver/two_phase_tables.cpp
        src/solver/cfop_last_layer.cpp
        src/solver/cfop.cpp
        src/solver/solver.cpp
        src/solver/stepped_solution.cpp
        src/nxn/geometry.cpp
        src/nxn/cube.cpp
        src/nxn/commutator.cpp
        src/nxn/cube3.cpp
        src/nxn/centres.cpp
        src/nxn/solution.cpp
        src/nxn/stages.cpp
        src/nxn/strategy.cpp
        src/nxn/two_by_two.cpp
        src/nxn/mesh.cpp
        src/nxn/notation.cpp
        src/nxn/stepped_plan.cpp
        src/nxn/outside_view.cpp
        src/nxn/validate.cpp
        src/nxn/scan_assembly.cpp
        src/vision/color_resolution.cpp
        src/vision/color_classification.cpp
        src/vision/recording_v2.cpp
        src/vision/prepared_frame.cpp
        src/vision/sticker_colour.cpp
        src/vision/direct_scan.cpp
        src/vision/quad_fit.cpp
        src/vision/corner_fit_n.cpp
        src/vision/corner_view.cpp
        src/vision/held_cube.cpp
        src/diagnostics/json.cpp
        src/diagnostics/describe.cpp
        src/diagnostics/record.cpp
        src/diagnostics/jsonl_file.cpp
        src/profile/qsg_timing.cpp
        src/profile/recorder.cpp
        src/profile/session.cpp
        src/i18n/i18n.cpp)

# What a process with no profiling switch does (cube/profile/session.hpp) is a
# build-configuration fact, so it reaches session.cpp as a compile definition
# computed by CMake (cmake/Tracy.cmake) and not as an #ifdef on NDEBUG.
set_source_files_properties(src/profile/session.cpp PROPERTIES
    COMPILE_DEFINITIONS "CUBE_PROFILE_DEFAULT_ON=${CUBEULATOR_PROFILE_DEFAULT_ON}")

# The Tracy GPU contexts (cube::profiling::GpuContext): built only when
# Tracy is on, because the per-backend headers they include (<d3d11.h> on
# Windows, Tracy's OpenGL and D3D11 bindings) are nobody else's business.
# The disabled and recorder variants of cube/profiling.hpp declare the same
# classes as no-ops, so nothing else notices. The Direct3D 11 backend is
# platform code: the platform's fragment names it in
# CUBEULATOR_PLATFORM_PROFILING_GPU_SOURCES (relative to src/app), and this
# file asks no question about the OS. Each file gets its third-party-header
# tokens as compile definitions: the GL tokens TracyOpenGL.hpp reads, and
# NOMINMAX for the D3D11 file's <windows.h>.
if(CUBEULATOR_ENABLE_TRACY)
    if(CUBEULATOR_PLATFORM_PROFILING_OPENGL)
        target_sources(cube_lib PRIVATE src/profile/tracy_gpu.cpp)
        set_source_files_properties(src/profile/tracy_gpu.cpp PROPERTIES
            COMPILE_DEFINITIONS
                "GL_TIMESTAMP=0x8E28;GL_QUERY_COUNTER_BITS=0x8864;GL_QUERY_RESULT=0x8866;GL_QUERY_RESULT_AVAILABLE=0x8867;GL_NO_ERROR=0x0;GL_EXTENSIONS=0x1F03;GL_FALSE=0x0;GL_VERSION_3_0=1")
    else()
        target_sources(cube_lib PRIVATE src/profile/tracy_gpu_opengl_none.cpp)
    endif()
    foreach(_gpu_source IN LISTS CUBEULATOR_PLATFORM_PROFILING_GPU_SOURCES)
        set(_gpu_file "${PROJECT_SOURCE_DIR}/src/app/${_gpu_source}")
        target_sources(cube_lib PRIVATE "${_gpu_file}")
        set_source_files_properties("${_gpu_file}" PROPERTIES
            COMPILE_DEFINITIONS "NOMINMAX")
    endforeach()

    # Vulkan is an API, not an operating system: the same file serves every
    # platform that has it, and the platform's fragment says whether that is
    # this one. Tracy resolves Vulkan's functions through two entry points the
    # application hands over, so only the headers are needed, not the loader.
    if(CUBEULATOR_PLATFORM_PROFILING_VULKAN)
        find_package(VulkanHeaders CONFIG REQUIRED)
        target_sources(cube_lib PRIVATE src/profile/tracy_gpu_vulkan.cpp)
        target_link_libraries(cube_lib PRIVATE Vulkan::Headers)
    else()
        target_sources(cube_lib PRIVATE src/profile/tracy_gpu_vulkan_none.cpp)
    endif()
endif()

# Two files make the compiler build lookup tables from loops: the two-phase
# solver's phase 1 move tables (two_phase_tables.cpp, about 85 thousand
# entries) and the last-layer algorithms, parsed from their text
# (cfop_last_layer.cpp). They fit inside every compiler's default
# constant-evaluation budget today - by a factor of only 1.5 to 2 on Clang and
# MSVC - but a budget is a count of steps a vendor chose, so these two files
# are given plenty of room rather than being one added move table away from a
# build error. Only these files: the budget exists to stop runaway evaluation,
# and everything else should keep that protection.
#
# The flag differs by compiler (clang-cl accepts the Clang spelling only
# through /clang:).
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(CUBEULATOR_CONSTEXPR_BUDGET "/clang:-fconstexpr-steps=10000000")
elseif(MSVC)
    set(CUBEULATOR_CONSTEXPR_BUDGET "/constexpr:steps10000000")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CUBEULATOR_CONSTEXPR_BUDGET "-fconstexpr-ops-limit=200000000")
else()
    set(CUBEULATOR_CONSTEXPR_BUDGET "-fconstexpr-steps=10000000")
endif()
set_source_files_properties(
    src/solver/two_phase_tables.cpp
    src/solver/cfop_last_layer.cpp
    PROPERTIES COMPILE_OPTIONS "${CUBEULATOR_CONSTEXPR_BUDGET}")

add_dependencies(cube_lib cube_version)

# Which cube/profiling.hpp the zone macros resolve to: CMake chooses the
# directory, not an #ifdef in the header (see cmake/Tracy.cmake and
# cmake/ProfileRecorder.cmake). PUBLIC because every layer above this one - the
# app's capture loop and controllers, the tracker wrapper, the tools - is
# instrumented with the same macros, and the real variant includes Tracy's own
# headers, which cube::tracy supplies.
if(CUBEULATOR_ENABLE_TRACY)
    set(CUBEULATOR_PROFILING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/variants/profiling-tracy_enabled")
elseif(CUBEULATOR_ENABLE_PROFILE_RECORDER)
    set(CUBEULATOR_PROFILING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/variants/profiling-recorder")
else()
    set(CUBEULATOR_PROFILING_DIR "${CMAKE_CURRENT_SOURCE_DIR}/variants/profiling-tracy_disabled")
endif()

target_include_directories(cube_lib
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>"
        "$<BUILD_INTERFACE:${CUBEULATOR_PROFILING_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/variants/simd-${CUBEULATOR_SIMD_RESOLVED}>"
        "$<INSTALL_INTERFACE:include>")

target_compile_features(cube_lib PUBLIC cxx_std_23)

target_link_libraries(cube_lib
    PUBLIC
        cube::opencv
        cube::tracy
    PRIVATE
        cube::warnings
        cube::sanitizers
        cube::coverage
        fmt::fmt)
