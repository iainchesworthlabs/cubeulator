# ---------------------------------------------------------------------------
# Hardening.cmake
#
# Defines the INTERFACE target `cube::hardening`: the compiler and linker
# options of icl-common-infra's IclHardening module (stack protector,
# _FORTIFY_SOURCE=3 in optimised builds, control-flow protection, relro and now,
# position-independent code; /GS /guard:cf /sdl on MSVC). Link it PRIVATE-ly
# into every first-party target that ships: the library, the platform layer and
# the app's libraries and executable. Tests and tools are not hardened.
#
# A platform that has none of these to ask for (the browser) says so in its
# fragment (CUBEULATOR_PLATFORM_HARDENING), and there the target is empty, so the
# link lines stay the same on every platform.
# ---------------------------------------------------------------------------

add_library(cube_hardening INTERFACE)
add_library(cube::hardening ALIAS cube_hardening)

if(CUBEULATOR_PLATFORM_HARDENING)
    include("${CMAKE_CURRENT_LIST_DIR}/icl/IclHardening.cmake")
    target_link_libraries(cube_hardening INTERFACE icl_hardening)
endif()
