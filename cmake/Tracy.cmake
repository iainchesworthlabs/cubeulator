# ---------------------------------------------------------------------------
# Tracy.cmake
#
# Defines an INTERFACE target `cube::tracy` that, when CUBEULATOR_ENABLE_TRACY
# is on, links Tracy's client library. The zone macros the sources use
# (cube/profiling.hpp) are a pair of identically-pathed headers under
# src/lib/variants/; src/lib/CMakeLists.txt puts exactly one of them on
# cube::lib's include path, chosen by this option and not by an #ifdef, so
# every instrumented call site compiles unchanged either way and the disabled
# variant is a set of empty macros: a build without Tracy pays nothing for the
# instrumentation and does not even need Tracy present.
#
# ON by default (settled in the top-level CMakeLists.txt, before project(),
# because it selects a vcpkg feature; the web, Android and iOS presets turn it
# OFF, so does a coverage build, and so must a Windows-on-ARM configure). ON
# does not mean running:
# the client is the overlay port's (vcpkg-overlay-ports/tracy), built for MANUAL LIFETIME and
# loopback-only listening, so a process that never calls
# cube::profiling::start() has no Tracy thread, no socket and no broadcast, and
# each zone is one relaxed atomic load. Whether the application starts it is
# cube::profile::Session's decision at launch (CUBEULATOR_PROFILE, --profile,
# and this file's CUBEULATOR_TRACY_AUTOSTART) - no recompile to turn it on in a
# release build. docs/profiling.md has the whole story.
#
# Tracy itself is a vcpkg manifest feature ("profiling" in vcpkg.json) that the
# top-level CMakeLists.txt adds to VCPKG_MANIFEST_FEATURES when this option is
# on. The *-profiling presets also set CUBEULATOR_TRACY_TOOLS, which adds the
# host-built command-line tools (tracy-capture, tracy-csvexport) so a trace can
# be recorded and dumped headlessly:
#
#   cmake --preset windows-msvc-profiling        # or -linux-gcc- / -macos-clang-
# ---------------------------------------------------------------------------

option(CUBEULATOR_ENABLE_TRACY
    "Build the Tracy profiler client into the application (started at run time, see docs/profiling.md)"
    OFF)

# Whether a process with no instruction starts the profiler: AUTO is "yes in a
# Debug build, no in every other configuration" (decided per configuration, so
# multi-config generators get it right); ON / OFF force it. The profiling
# presets force it ON: their whole point is a trace.
set(CUBEULATOR_TRACY_AUTOSTART "AUTO" CACHE STRING
    "Does a process started with no profiling switch start Tracy? AUTO = Debug builds only")
set_property(CACHE CUBEULATOR_TRACY_AUTOSTART PROPERTY STRINGS AUTO ON OFF)
if(CUBEULATOR_TRACY_AUTOSTART STREQUAL "AUTO")
    set(CUBEULATOR_PROFILE_DEFAULT_ON "$<CONFIG:Debug>")
elseif(CUBEULATOR_TRACY_AUTOSTART)
    set(CUBEULATOR_PROFILE_DEFAULT_ON "1")
else()
    set(CUBEULATOR_PROFILE_DEFAULT_ON "0")
endif()
if(NOT CUBEULATOR_ENABLE_TRACY)
    # Nothing to start in a build without a client.
    set(CUBEULATOR_PROFILE_DEFAULT_ON "0")
endif()

add_library(cube_tracy INTERFACE)
add_library(cube::tracy ALIAS cube_tracy)

if(CUBEULATOR_ENABLE_TRACY)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(cube_tracy INTERFACE Tracy::TracyClient)

    # Tracy 0.14 flipped TRACY_ENABLE off by default in its own CMake, and the
    # vcpkg port passes it to the client library's build only - the exported
    # Tracy::TracyClient target carries TRACY_ON_DEMAND,
    # TRACY_NO_CRASH_HANDLER and TRACY_IMPORTS, but not TRACY_ENABLE. Without
    # it every zone macro in a consumer compiles to nothing. It is the
    # client's compile-time master switch and must be defined where the
    # headers are read, which is exactly this INTERFACE target's job.
    target_compile_definitions(cube_tracy INTERFACE TRACY_ENABLE)

    # A number taken from a coverage build, or from an AddressSanitizer one,
    # is not a number about the program. Not an error - Debug builds profile
    # by default now, and someone chasing a bug in the profiling build itself
    # wants exactly that - but nothing else will say so.
    if(CUBEULATOR_ENABLE_COVERAGE)
        message(WARNING
            "CUBEULATOR_ENABLE_TRACY is on for a coverage build. Coverage "
            "instrumentation dominates the timings Tracy reports; use a "
            "RelWithDebInfo build (the *-profiling presets) for numbers worth "
            "reading.")
    endif()
endif()
