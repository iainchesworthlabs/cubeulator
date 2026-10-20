#pragma once

// The 128-bit member of the SIMD seam: per-pixel channel kernels written
// against OpenCV's universal intrinsics, which are SSE2 on x86-64, NEON on
// arm64 and simd128 in a browser. Sixteen pixels a step, the rest of the row
// by the scalar form.
//
// Which directory is on the include path is CMake's decision (cmake/Simd.cmake),
// and it picks this one only for a target whose compiler is already generating
// 128-bit vector code - SSE2 and NEON are part of their architectures, and the
// page that runs a build with -msimd128 already requires WebAssembly SIMD - so
// nothing here has to ask the CPU what it can do. What is asked, once a call, is
// cv::useOptimized(): the switch OpenCV's own run-time dispatch answers to
// (cv::setUseOptimized(false)), so that one switch sends the whole reader down
// its scalar paths, and a test can hold the two against each other in one
// binary.
//
// Every operation is an exact integer max, min or subtraction per byte, so the
// result is the scalar form's byte for byte, on every platform. That is checked
// by tests/unit/channel_kernels_test.cpp against OpenCV's own split/max/min, and
// over recorded sessions by `cube_scan digest` (docs/performance.md).
//
// No wider tier (AVX2, AVX-512) is here, because there is nothing to gain from
// one: the 128-bit kernel reads a 640 x 360 frame in 0.045 ms against a memory
// floor of about 0.02, and a wider one would buy at most 0.02 ms a frame at the
// price of a CPU probe per compiler. docs/performance.md has the measurement; if
// a kernel ever does want one, cv::checkHardwareSupport() answers the question
// at run time without a probe of our own.

#include <cube/vision/detail/channel_scalar.hpp>

#include <opencv2/core.hpp>
#include <opencv2/core/hal/intrin.hpp>
#include <opencv2/core/utility.hpp>

#include <string_view>

namespace cube::vision::detail {

inline constexpr std::string_view kSimdName = "vector128";

inline void max_of_channels(const cv::Mat& bgr, cv::Mat& value) {
    value.create(bgr.size(), CV_8UC1);
    const bool vectors = cv::useOptimized();
    constexpr int kLanes = 16;
    for (int row = 0; row < bgr.rows; ++row) {
        const unsigned char* in = bgr.ptr<unsigned char>(row);
        unsigned char* out = value.ptr<unsigned char>(row);
        int done = 0;
        if (vectors) {
            for (; done + kLanes <= bgr.cols; done += kLanes) {
                cv::v_uint8x16 blue;
                cv::v_uint8x16 green;
                cv::v_uint8x16 red;
                cv::v_load_deinterleave(in + (3 * done), blue, green, red);
                cv::v_store(out + done, cv::v_max(cv::v_max(blue, green), red));
            }
        }
        max_of_channels_scalar(in + (3 * done), out + done, bgr.cols - done);
    }
}

inline void extremes_of_channels(const cv::Mat& bgr, cv::Mat& value, cv::Mat& chroma) {
    value.create(bgr.size(), CV_8UC1);
    chroma.create(bgr.size(), CV_8UC1);
    const bool vectors = cv::useOptimized();
    constexpr int kLanes = 16;
    for (int row = 0; row < bgr.rows; ++row) {
        const unsigned char* in = bgr.ptr<unsigned char>(row);
        unsigned char* out_value = value.ptr<unsigned char>(row);
        unsigned char* out_chroma = chroma.ptr<unsigned char>(row);
        int done = 0;
        if (vectors) {
            for (; done + kLanes <= bgr.cols; done += kLanes) {
                cv::v_uint8x16 blue;
                cv::v_uint8x16 green;
                cv::v_uint8x16 red;
                cv::v_load_deinterleave(in + (3 * done), blue, green, red);
                const cv::v_uint8x16 highest = cv::v_max(cv::v_max(blue, green), red);
                const cv::v_uint8x16 lowest = cv::v_min(cv::v_min(blue, green), red);
                cv::v_store(out_value + done, highest);
                cv::v_store(out_chroma + done, cv::v_sub(highest, lowest));
            }
        }
        extremes_of_channels_scalar(in + (3 * done), out_value + done, out_chroma + done,
                                    bgr.cols - done);
    }
}

}  // namespace cube::vision::detail
