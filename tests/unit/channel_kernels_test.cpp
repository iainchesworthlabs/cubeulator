// The per-pixel channel kernels (cube/vision/detail/channel_kernels.hpp) against
// OpenCV's own split, max, min and subtract. Whichever variant CMake built the
// library with - 128-bit vectors where the target has them, scalar otherwise -
// must give those images byte for byte, on a width that is not a multiple of a
// vector, on a window into a larger image, and with OpenCV's run-time switch
// off, which sends a vector build down its scalar loops.

#include <catch2/catch_test_macros.hpp>

#include <cube/vision/detail/channel_kernels.hpp>

#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>

#include <array>
#include <cstdint>
#include <string>

namespace {

// OpenCV's switch for its own run-time dispatch, put back on leaving a case.
struct OptimizedUntilScopeEnds {
    bool before = cv::useOptimized();
    explicit OptimizedUntilScopeEnds(bool on) { cv::setUseOptimized(on); }
    OptimizedUntilScopeEnds(const OptimizedUntilScopeEnds&) = delete;
    OptimizedUntilScopeEnds& operator=(const OptimizedUntilScopeEnds&) = delete;
    OptimizedUntilScopeEnds(OptimizedUntilScopeEnds&&) = delete;
    OptimizedUntilScopeEnds& operator=(OptimizedUntilScopeEnds&&) = delete;
    ~OptimizedUntilScopeEnds() { cv::setUseOptimized(before); }
};

cv::Mat random_bgr(cv::Size size, std::uint64_t seed) {
    cv::Mat image(size, CV_8UC3);
    cv::RNG rng(seed);
    rng.fill(image, cv::RNG::UNIFORM, cv::Scalar::all(0), cv::Scalar::all(256));
    return image;
}

void check_against_opencv(const cv::Mat& bgr) {
    std::array<cv::Mat, 3> planes;
    cv::split(bgr, planes.data());
    cv::Mat highest;
    cv::Mat lowest;
    cv::Mat chroma;
    cv::max(cv::max(planes.at(0), planes.at(1)), planes.at(2), highest);
    cv::min(cv::min(planes.at(0), planes.at(1)), planes.at(2), lowest);
    cv::subtract(highest, lowest, chroma);

    cv::Mat value;
    cube::vision::detail::max_of_channels(bgr, value);
    CHECK(value.size() == bgr.size());
    CHECK(value.type() == CV_8UC1);
    CHECK(cv::countNonZero(value != highest) == 0);

    cv::Mat both_value;
    cv::Mat both_chroma;
    cube::vision::detail::extremes_of_channels(bgr, both_value, both_chroma);
    CHECK(cv::countNonZero(both_value != highest) == 0);
    CHECK(cv::countNonZero(both_chroma != chroma) == 0);
}

}  // namespace

TEST_CASE("the channel kernels match OpenCV's, whatever the variant", "[channel_kernels]") {
    INFO("variant: " << std::string(cube::vision::detail::kSimdName));
    for (const bool optimized : {true, false}) {
        const OptimizedUntilScopeEnds guard(optimized);
        CAPTURE(optimized);
        // Widths either side of the vector length, a width of one pixel, a row
        // shorter than a vector, and the working frame itself.
        for (const int width : {1, 2, 15, 16, 17, 31, 32, 33, 47, 64, 100, 640}) {
            CAPTURE(width);
            check_against_opencv(random_bgr(cv::Size(width, 7), 1000 + width));
        }
        check_against_opencv(random_bgr(cv::Size(641, 361), 7));
    }
}

TEST_CASE("the channel kernels take the extremes at the ends of the range", "[channel_kernels]") {
    cv::Mat bgr(cv::Size(40, 4), CV_8UC3, cv::Scalar(0, 0, 0));
    bgr.row(1).setTo(cv::Scalar(255, 255, 255));
    bgr.row(2).setTo(cv::Scalar(255, 0, 0));
    bgr.row(3).setTo(cv::Scalar(3, 254, 128));
    check_against_opencv(bgr);

    cv::Mat value;
    cv::Mat chroma;
    cube::vision::detail::extremes_of_channels(bgr, value, chroma);
    CHECK(value.at<unsigned char>(0, 5) == 0);
    CHECK(chroma.at<unsigned char>(0, 5) == 0);
    CHECK(value.at<unsigned char>(1, 39) == 255);
    CHECK(chroma.at<unsigned char>(1, 39) == 0);
    CHECK(chroma.at<unsigned char>(2, 20) == 255);
    CHECK(value.at<unsigned char>(3, 17) == 254);
    CHECK(chroma.at<unsigned char>(3, 17) == 251);
}

TEST_CASE("the channel kernels read a window into a larger image as an image of its own",
          "[channel_kernels]") {
    // The rows of a window are not contiguous: a walk that assumed they were
    // would read the wrong pixels from the second row on.
    const cv::Mat whole = random_bgr(cv::Size(120, 40), 99);
    const cv::Mat window = whole(cv::Rect(7, 3, 83, 29));
    REQUIRE_FALSE(window.isContinuous());
    check_against_opencv(window);
    check_against_opencv(window.clone());
}

TEST_CASE("the channel kernels name the variant CMake built", "[channel_kernels]") {
    const std::string name(cube::vision::detail::kSimdName);
    CHECK((name == "generic" || name == "vector128"));
}
