# ---------------------------------------------------------------------------
# Fuzz harnesses for the code that reads input a person does not control: a
# move notation, a state string, a recording's index and manifest, a camera
# frame (docs/testing.md, "Fuzzing"). Each harness is one function,
# LLVMFuzzerTestOneInput, in fuzz_<name>.cpp, with a seed corpus in
# seeds/<name>/ and the inputs that once crashed it in regressions/<name>/.
#
# Two ways to run one:
#
# - Replay (any compiler): the harness links replay_main.cpp, which hands every
#   file of the directories on its command line to the function once. ctest runs
#   that over the seeds and the regressions, under the sanitizers of the preset,
#   as the tests labelled `fuzz`. It takes seconds and is part of the pull
#   request gate.
# - Fuzz (Clang): with CUBEULATOR_FUZZ_LIBFUZZER the harness links libFuzzer
#   instead, and the same binary mutates the corpus. The `linux-clang-fuzz`
#   preset builds the library with the instrumentation libFuzzer needs.
# ---------------------------------------------------------------------------

option(CUBEULATOR_FUZZ_LIBFUZZER
    "Link the fuzz harnesses to libFuzzer (Clang) instead of the replay driver" OFF)

function(cube_add_fuzzer name)
    set(target "fuzz_${name}")
    add_executable(${target} "fuzz_${name}.cpp")
    target_link_libraries(${target} PRIVATE cube::lib cube::warnings)
    if(CUBEULATOR_FUZZ_LIBFUZZER)
        target_link_options(${target} PRIVATE -fsanitize=fuzzer)
    else()
        target_sources(${target} PRIVATE replay_main.cpp)
    endif()
    cube_deploy_sanitizer_runtime(${target})

    # -runs=0: libFuzzer replays the corpus and stops; the replay driver ignores it.
    add_test(NAME fuzz.${name}
        COMMAND ${target}
            "${CMAKE_CURRENT_SOURCE_DIR}/seeds/${name}"
            "${CMAKE_CURRENT_SOURCE_DIR}/regressions/${name}"
            -runs=0)
    set_tests_properties(fuzz.${name} PROPERTIES LABELS "fuzz" TIMEOUT 120)
endfunction()

cube_add_fuzzer(notation)
cube_add_fuzzer(state_string)
cube_add_fuzzer(recording_index)
cube_add_fuzzer(manifest)
cube_add_fuzzer(frame)
