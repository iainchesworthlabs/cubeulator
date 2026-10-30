// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// The side and corner readers (cube::vision::read_side and read_corner) on any picture. The
// first three bytes pick the width, the height and the cube's size; the rest are pixels, read
// as 8-bit BGR and repeated to fill the frame. A camera frame is the input a person does not
// control, and most of what it holds is not a cube.
//
// The frame is at most 128 x 128, a little more than the 115 x 112 that once made read_side take
// tens of seconds (issue 276, regressions/frame): a larger frame only repeats the same pixels
// for more time per run.

#include <cube/vision/corner_view.hpp>
#include <cube/vision/direct_scan.hpp>

#include <opencv2/core.hpp>

#include <cstddef>
#include <cstdint>

namespace {

constexpr std::size_t kHeader = 3;
constexpr int kLargest = 128;

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size <= kHeader) {
        return 0;
    }
    const int width = 1 + (data[0] % kLargest);
    const int height = 1 + (data[1] % kLargest);
    const int cube_size = 2 + (data[2] % 8);
    const std::size_t pixel_bytes = size - kHeader;

    cv::Mat frame(height, width, CV_8UC3);
    std::size_t next = 0;
    for (int row = 0; row < height; ++row) {
        auto* line = frame.ptr<std::uint8_t>(row);
        for (int i = 0; i < width * 3; ++i) {
            line[i] = data[kHeader + (next % pixel_bytes)];
            ++next;
        }
    }

    (void)cube::vision::read_side(frame, cube_size);
    (void)cube::vision::read_corner(frame, cube_size);
    return 0;
}
