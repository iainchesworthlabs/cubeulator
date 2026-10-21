# Script-mode CTest case (cmake -P): cmake/VcpkgRoot.cmake picks the vcpkg root
# the way its header says it does.
#
# Nothing here touches the network or the real deps/vcpkg. A fake upstream
# repository stands in for the vcpkg fork (one file, a toolchain that prints a
# line), a fake superproject pins it as deps/vcpkg, and a tiny project that
# calls cubeulator_resolve_vcpkg() before project() is configured against them
# in each of the situations that matter:
#
#   store         first use makes <store>/<pin>, the second configure reuses it
#   ICL store     ICL_VCPKG_STORE does the same; CUBEULATOR_VCPKG_STORE wins
#                 when both are set
#   other fork    a pin that is not in the mirror's origin is fetched from the
#                 URL in .gitmodules, so repositories with different forks can
#                 share one store
#   pin moved     an old build directory is told to configure --fresh (CMake
#                 would otherwise keep the old toolchain), and does after it
#   root          CUBEULATOR_VCPKG_ROOT at another commit warns, does not fail
#   dead toolchain a CMAKE_TOOLCHAIN_FILE that does not exist says so
#   dead link     deps/vcpkg as a link whose target is gone says so, and is
#                 never initialised over (what a link points at is shared)
#
# Run: cmake -DREPO_ROOT=<repo> -DWORK_DIR=<scratch dir> -P check_vcpkg_root.cmake

if(NOT DEFINED REPO_ROOT OR NOT DEFINED WORK_DIR)
    message(FATAL_ERROR "Pass -DREPO_ROOT=<repo> -DWORK_DIR=<scratch dir>.")
endif()
find_package(Git REQUIRED)

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")
set(upstream "${WORK_DIR}/upstream")
set(super "${WORK_DIR}/super")
set(proj "${WORK_DIR}/proj")
set(store "${WORK_DIR}/store")

function(git dir)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -c user.name=test -c user.email=test@example.invalid
            -c commit.gpgsign=false -C "${dir}" ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "git ${ARGN} in ${dir} failed:\n${out}")
    endif()
    set(GIT_OUT "${out}" PARENT_SCOPE)
endfunction()

# A commit of the fake upstream; returns its hash in `out`.
function(upstream_commit label out)
    file(WRITE "${upstream}/scripts/buildsystems/vcpkg.cmake" "message(STATUS \"fake-vcpkg ${label}\")\n")
    git("${upstream}" add -A)
    git("${upstream}" commit -q -m "${label}")
    git("${upstream}" rev-parse HEAD)
    set(${out} "${GIT_OUT}" PARENT_SCOPE)
endfunction()

function(pin_super sha)
    git("${super}" update-index --add --cacheinfo "160000,${sha},deps/vcpkg")
endfunction()

file(MAKE_DIRECTORY "${upstream}" "${super}" "${proj}")
git("${upstream}" init -q)
upstream_commit("one" pin1)
git("${super}" init -q)
file(WRITE "${super}/.gitmodules"
    "[submodule \"deps/vcpkg\"]\n\tpath = deps/vcpkg\n\turl = ${upstream}\n")
pin_super("${pin1}")
file(WRITE "${proj}/CMakeLists.txt" [[
cmake_minimum_required(VERSION 3.28)
include("${REPO}/cmake/VcpkgRoot.cmake")
cubeulator_resolve_vcpkg("${SUPER}")
project(t LANGUAGES NONE)
message(STATUS "root-dir=${CUBEULATOR_VCPKG_ROOT_DIR}")
]])

# Configures the scratch project; the result is in rc and log. `env` is a list
# of NAME=value for the child; every CUBEULATOR_VCPKG_* not in it is unset.
function(configure build env)
    set(unset)
    foreach(name CUBEULATOR_VCPKG_ROOT CUBEULATOR_VCPKG_STORE CUBEULATOR_VCPKG_MIRROR
                ICL_VCPKG_STORE ICL_VCPKG_MIRROR)
        list(APPEND unset "--unset=${name}")
    endforeach()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env ${unset} ${env}
            "${CMAKE_COMMAND}" -S "${proj}" -B "${WORK_DIR}/${build}" -G "${GEN}"
            "-DREPO=${REPO_ROOT}" "-DSUPER=${super}" ${ARGN}
        RESULT_VARIABLE rc OUTPUT_VARIABLE log ERROR_VARIABLE log)
    set(rc "${rc}" PARENT_SCOPE)
    set(log "${log}" PARENT_SCOPE)
endfunction()

function(expect_ok what)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "${what}: the configure failed.\n${log}")
    endif()
endfunction()

# CMake wraps long error text at 80 columns, so the output is compared with its
# runs of white space collapsed to one space.
function(_flat_log out)
    string(REGEX REPLACE "[ 	
]+" " " flat "${log}")
    set(${out} "${flat}" PARENT_SCOPE)
endfunction()

function(expect_in_log what text)
    _flat_log(flat)
    string(FIND "${flat}" "${text}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "${what}: expected '${text}' in the output.\n${log}")
    endif()
endfunction()

function(expect_not_in_log what text)
    _flat_log(flat)
    string(FIND "${flat}" "${text}" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "${what}: did not expect '${text}' in the output.\n${log}")
    endif()
endfunction()

set(GEN "${CMAKE_GENERATOR}")
if(GEN STREQUAL "")
    set(GEN "Ninja")
endif()
string(SUBSTRING "${pin1}" 0 12 short1)

# --- store: first use creates the checkout, the second reuses it ----------
configure(b-store1 "CUBEULATOR_VCPKG_STORE=${store}")
expect_ok("store, first use")
expect_in_log("store, first use" "fake-vcpkg one")
expect_in_log("store, first use" "root-dir=${store}/${short1}")
if(NOT EXISTS "${store}/${short1}/.cubeulator-ready")
    message(FATAL_ERROR "store: ${store}/${short1} was not marked ready.")
endif()

configure(b-store2 "CUBEULATOR_VCPKG_STORE=${store}")
expect_ok("store, second build directory")
expect_not_in_log("store, second build directory" "Checking out vcpkg")
expect_in_log("store, second build directory" "root-dir=${store}/${short1}")

# --- pin moved: an old build directory says to start fresh ---------------
upstream_commit("two" pin2)
pin_super("${pin2}")
string(SUBSTRING "${pin2}" 0 12 short2)
configure(b-store1 "CUBEULATOR_VCPKG_STORE=${store}")
if(rc EQUAL 0)
    message(FATAL_ERROR "pin moved: an old build directory must not keep the old toolchain silently.")
endif()
expect_in_log("pin moved" "--fresh")
configure(b-store1 "CUBEULATOR_VCPKG_STORE=${store}" --fresh)
expect_ok("pin moved, --fresh")
expect_in_log("pin moved, --fresh" "fake-vcpkg two")
expect_in_log("pin moved, --fresh" "root-dir=${store}/${short2}")
if(NOT EXISTS "${store}/${short1}/.cubeulator-ready")
    message(FATAL_ERROR "pin moved: the checkout of the old pin must be left alone.")
endif()

# --- ICL_VCPKG_STORE, and the project's own setting wins ------------------
set(store_icl "${WORK_DIR}/store-icl")
configure(b-icl "ICL_VCPKG_STORE=${store_icl}")
expect_ok("ICL_VCPKG_STORE")
expect_in_log("ICL_VCPKG_STORE" "root-dir=${store_icl}/${short2}")
configure(b-both "ICL_VCPKG_STORE=${store_icl}-unused;CUBEULATOR_VCPKG_STORE=${store}")
expect_ok("both stores set")
expect_in_log("both stores set" "root-dir=${store}/${short2}")
if(EXISTS "${store_icl}-unused")
    message(FATAL_ERROR "both stores set: ICL_VCPKG_STORE must be ignored when CUBEULATOR_VCPKG_STORE is set.")
endif()

# --- other fork: the pin is in a repository the mirror does not know -----
set(upstream_b "${WORK_DIR}/upstream-b")
set(super_b "${WORK_DIR}/super-b")
file(MAKE_DIRECTORY "${upstream_b}" "${super_b}")
git("${upstream_b}" init -q)
set(upstream_save "${upstream}")
set(upstream "${upstream_b}")
upstream_commit("other fork" pin_b)
set(upstream "${upstream_save}")
git("${super_b}" init -q)
file(WRITE "${super_b}/.gitmodules"
    "[submodule \"deps/vcpkg\"]
	path = deps/vcpkg
	url = ${upstream_b}
")
git("${super_b}" update-index --add --cacheinfo "160000,${pin_b},deps/vcpkg")
string(SUBSTRING "${pin_b}" 0 12 short_b)
configure(b-other "CUBEULATOR_VCPKG_STORE=${store}" "-DSUPER=${super_b}")
expect_ok("other fork")
expect_in_log("other fork" "fake-vcpkg other fork")
expect_in_log("other fork" "root-dir=${store}/${short_b}")

# --- root: another commit than the pin is a warning ----------------------
configure(b-root "CUBEULATOR_VCPKG_ROOT=${store}/${short1}")
expect_ok("root at another commit")
expect_in_log("root at another commit" "deps/vcpkg pins ${pin2}")
expect_in_log("root at another commit" "root-dir=${store}/${short1}")

# --- dead toolchain -------------------------------------------------------
configure(b-dead "" "-DCMAKE_TOOLCHAIN_FILE=${WORK_DIR}/nowhere/scripts/buildsystems/vcpkg.cmake")
if(rc EQUAL 0)
    message(FATAL_ERROR "dead toolchain: the configure should have failed.")
endif()
expect_in_log("dead toolchain" "does not exist")

# --- dead link at deps/vcpkg ---------------------------------------------
# The link is a symbolic link where the machine lets this user make one, and
# otherwise a directory junction (what the repo's Windows developers have),
# which needs no privilege. Which of the two is found out by trying, not by
# asking what the OS is: that is a CMake decision made elsewhere.
set(link "${super}/deps/vcpkg")
set(target "${WORK_DIR}/link-target")
file(MAKE_DIRECTORY "${super}/deps" "${target}")
file(CREATE_LINK "${target}" "${link}" SYMBOLIC RESULT made)
if(NOT made STREQUAL "0")
    find_program(CMD_EXECUTABLE cmd)
    if(NOT CMD_EXECUTABLE)
        message(FATAL_ERROR "dead link: cannot make a symbolic link (${made}) and there is no cmd to make a junction.")
    endif()
    string(REPLACE "/" "\\" link_native "${link}")
    string(REPLACE "/" "\\" target_native "${target}")
    execute_process(COMMAND cmd /c mklink /J "${link_native}" "${target_native}"
        RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
    if(NOT rc EQUAL 0)
        message(FATAL_ERROR "dead link: could not make a junction at ${link}.")
    endif()
endif()
file(REMOVE_RECURSE "${target}")
configure(b-link "")
if(rc EQUAL 0)
    message(FATAL_ERROR "dead link: the configure should have failed.")
endif()
expect_in_log("dead link" "is a link to")
expect_in_log("dead link" "does not exist")
expect_not_in_log("dead link" "Initialising vendored vcpkg submodule")
file(REMOVE "${link}")
if(IS_SYMLINK "${link}" AND CMD_EXECUTABLE)
    execute_process(COMMAND cmd /c rmdir "${link_native}" OUTPUT_QUIET ERROR_QUIET)
endif()

message(STATUS "vcpkg root selection behaves as documented")
