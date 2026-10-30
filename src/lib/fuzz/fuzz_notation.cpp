// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// The move notation parser (cube::nxn::parse) on any text and any size. The first byte picks
// the size, including ones the library does not build; the rest is the text. A text that parses
// must print as notation that parses back to a text that prints the same.

#include <cube/nxn/notation.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size == 0) {
        return 0;
    }
    const int cube_size = data[0] % 12;
    const std::string_view text(reinterpret_cast<const char*>(data) + 1, size - 1);

    const auto moves = cube::nxn::parse(text, cube_size);
    if (!moves) {
        return 0;
    }
    const std::string printed = cube::nxn::to_string(*moves, cube_size);
    const auto again = cube::nxn::parse(printed, cube_size);
    if (!again || cube::nxn::to_string(*again, cube_size) != printed) {
        std::abort();
    }
    return 0;
}
