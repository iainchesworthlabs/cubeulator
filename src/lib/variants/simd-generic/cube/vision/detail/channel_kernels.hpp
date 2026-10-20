#pragma once

// The scalar member of the SIMD seam. See the simd-vector128 sibling's
// identically-pathed cube/vision/detail/channel_kernels.hpp for what the seam
// is, and cmake/Simd.cmake for how CMake chooses between the two (never an
// #ifdef): this one is picked on a target with no 128-bit vector unit the
// build can count on, and by -DCUBEULATOR_SIMD=generic, which is also the
// reference a reproducibility comparison should reach for first.

#include <cube/vision/detail/channel_scalar.hpp>

#include <opencv2/core.hpp>

#include <string_view>

namespace cube::vision::detail {

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic): kernels over image rows.

inline constexpr std::string_view kSimdName = "generic";

// `value` (created) is the brightest channel of every pixel of the 8-bit BGR
// image `bgr`.
inline void max_of_channels(const cv::Mat& bgr, cv::Mat& value) {
    value.create(bgr.size(), CV_8UC1);
    for (int row = 0; row < bgr.rows; ++row) {
        max_of_channels_scalar(bgr.ptr<unsigned char>(row), value.ptr<unsigned char>(row),
                               bgr.cols);
    }
}

// `value` as above, and `chroma` (created): the brightest channel less the
// darkest.
inline void extremes_of_channels(const cv::Mat& bgr, cv::Mat& value, cv::Mat& chroma) {
    value.create(bgr.size(), CV_8UC1);
    chroma.create(bgr.size(), CV_8UC1);
    for (int row = 0; row < bgr.rows; ++row) {
        extremes_of_channels_scalar(bgr.ptr<unsigned char>(row), value.ptr<unsigned char>(row),
                                    chroma.ptr<unsigned char>(row), bgr.cols);
    }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

}  // namespace cube::vision::detail
