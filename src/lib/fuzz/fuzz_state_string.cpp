// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// The state-string oracle (cube::vision::recording::state_string_problem) on any text and any
// size: the check that a pasted or recorded cube is one before anything is built from it. The
// first byte picks the size, including ones outside 2 to 9.

#include <cube/vision/recording_v2.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }
    const int cube_size = data[0] % 12;
    const std::string state(reinterpret_cast<const char*>(data) + 1, size - 1);
    (void)cube::vision::recording::state_string_problem(state, cube_size);
    return 0;
}
