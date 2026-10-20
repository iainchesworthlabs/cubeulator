#pragma once

// The scalar form of the per-pixel channel kernels, written once: it is the
// whole of the `simd-generic` variant, and the tail and the run-time fallback
// of every other (see cube/vision/detail/channel_kernels.hpp, which CMake
// chooses). It is also the reference the vector forms are held to: they are
// integer max, min and subtraction, so they match it exactly, not within a
// tolerance.

#include <algorithm>

namespace cube::vision::detail {

// The loops are over raw rows on purpose, and with the nested max and min (MSVC does
// not flatten the initializer-list form: 1.5 ms against 0.35 for the same loop).
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic,modernize-min-max-use-initializer-list)

// `pixels` interleaved BGR triples to the brightest channel of each.
inline void max_of_channels_scalar(const unsigned char* bgr, unsigned char* value, int pixels) {
    for (int i = 0; i < pixels; ++i, bgr += 3) {
        value[i] = std::max(std::max(bgr[0], bgr[1]), bgr[2]);
    }
}

// The brightest channel of each pixel, and how far it is above the darkest.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the two output planes, value then chroma.
inline void extremes_of_channels_scalar(const unsigned char* bgr, unsigned char* value,
                                        unsigned char* chroma, int pixels) {
    for (int i = 0; i < pixels; ++i, bgr += 3) {
        const unsigned char highest = std::max(std::max(bgr[0], bgr[1]), bgr[2]);
        const unsigned char lowest = std::min(std::min(bgr[0], bgr[1]), bgr[2]);
        value[i] = highest;
        chroma[i] = static_cast<unsigned char>(highest - lowest);
    }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic,modernize-min-max-use-initializer-list)

}  // namespace cube::vision::detail
