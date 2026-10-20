// The looks that take their outlines from the colour channels of the working
// frame (prepared_frame.hpp), against the same looks written with OpenCV's own
// split, max and min. They are walked pixel by pixel in the library, for the
// allocations a split costs, and these are what says that walk is the one
// arithmetic: the outlines are the same, point for point.

#include <catch2/catch_test_macros.hpp>

#include "drawn_cube.hpp"

#include <cube/solver/cube.hpp>
#include <cube/vision/prepared_frame.hpp>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

using cube::solver::Color;
using Contours = std::vector<std::vector<cv::Point>>;

namespace {

// Threshold and trace, as prepared_frame.cpp does them: a block 0.12 of the
// frame's smaller side, odd, 8 below the mean; opened by a 3 x 3.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): the plane, then what is taken out of it.
Contours traced_like_the_library(const cv::Mat& plane, const cv::Mat& taken_out) {
    const int block = std::max(
        3, static_cast<int>(static_cast<unsigned>(std::min(plane.rows, plane.cols) * 0.12) | 1U));
    cv::Mat binary;
    cv::adaptiveThreshold(plane, binary, 255.0, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY,
                          block, 8.0);
    if (!taken_out.empty()) {
        cv::Mat kept;
        cv::bitwise_not(taken_out, kept);
        cv::bitwise_and(binary, kept, binary);
    }
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, cv::Mat::ones(3, 3, CV_8U));
    Contours contours;
    cv::findContours(binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);
    return contours;
}

struct Planes {
    cv::Mat value;
    cv::Mat chroma;
};

Planes planes_of(const cv::Mat& working) {
    std::array<cv::Mat, 3> bgr;
    cv::split(working, bgr.data());
    Planes planes;
    cv::Mat lowest;
    cv::max(cv::max(bgr.at(0), bgr.at(1)), bgr.at(2), planes.value);
    cv::min(cv::min(bgr.at(0), bgr.at(1)), bgr.at(2), lowest);
    cv::subtract(planes.value, lowest, planes.chroma);
    return planes;
}

Contours without_glare_by_opencv(const cv::Mat& working) {
    const Planes planes = planes_of(working);
    // The brightness the outlines are taken from is blurred, and so is the one
    // "clipped" is judged on: the look has always cut out what is clipped after
    // the blur (colourless, by the unblurred chroma), and a read depends on it.
    cv::Mat brightness;
    cv::GaussianBlur(planes.value, brightness, cv::Size(5, 5), 0.0);
    cv::Mat bright;
    cv::Mat grey;
    cv::compare(brightness, 252, bright, cv::CMP_GE);
    cv::compare(planes.chroma, 12, grey, cv::CMP_LT);
    cv::Mat mask;
    cv::bitwise_and(bright, grey, mask);
    cv::dilate(mask, mask, cv::Mat::ones(3, 3, CV_8U));
    return traced_like_the_library(brightness, mask);
}

Contours chroma_by_opencv(const cv::Mat& working) {
    Planes planes = planes_of(working);
    cv::Mat chroma;
    cv::divide(planes.chroma, planes.value, chroma, 255.0);
    cv::GaussianBlur(chroma, chroma, cv::Size(5, 5), 0.0);
    return traced_like_the_library(chroma, cv::Mat{});
}

// A side with a highlight across the plastic between two rows of stickers, a
// clipped patch on one sticker, and a little noise everywhere, so that the
// channels differ pixel to pixel and a wrong one cannot match by luck.
cv::Mat glared_side(cv::Size size) {
    const std::array<Color, 9> stickers = {
        Color::White,  Color::Red, Color::Blue,   Color::Green, Color::White,
        Color::Orange, Color::Red, Color::Yellow, Color::Blue,
    };
    const float scale = static_cast<float>(size.width) / 640.0F;
    const cube::test::Pose pose{
        .centre = {static_cast<float>(size.width) / 2.0F, static_cast<float>(size.height) / 2.0F},
        .pitch = 40.0F * scale,
        .degrees = 5.0F,
    };
    cv::Mat frame = cube::test::draw_side(stickers, pose, cube::test::Drawing{.size = size});
    const int pitch = static_cast<int>(40.0F * scale);
    const int left = (size.width / 2) - (2 * pitch);
    const int top = (size.height / 2) - (pitch / 2);
    cv::rectangle(frame, cv::Rect(left, top, 4 * pitch, pitch / 4), cv::Scalar(255, 255, 255),
                  cv::FILLED);
    const int spot_left = (size.width / 2) + (pitch / 4);
    const int spot_top = (size.height / 2) + pitch;
    cv::rectangle(frame, cv::Rect(spot_left, spot_top, pitch / 3, pitch / 3),
                  cv::Scalar(254, 253, 255), cv::FILLED);
    cv::Mat noise(frame.size(), frame.type());
    cv::RNG rng(11);
    rng.fill(noise, cv::RNG::NORMAL, cv::Scalar::all(0), cv::Scalar::all(6));
    cv::add(frame, noise, frame);
    return frame;
}

}  // namespace

TEST_CASE("the glare look's outlines are those of the same look taken with OpenCV's own channels",
          "[prepared_frame]") {
    for (const cv::Size size : {cv::Size(640, 360), cv::Size(641, 361), cv::Size(317, 211)}) {
        CAPTURE(size.width, size.height);
        const cv::Mat working = glared_side(size);
        const Contours expected = without_glare_by_opencv(working);
        CHECK_FALSE(expected.empty());
        CHECK(cube::vision::contours_without_glare(working) == expected);
    }
}

TEST_CASE("the chroma look's outlines are those of the same look taken with OpenCV's own channels",
          "[prepared_frame]") {
    for (const cv::Size size : {cv::Size(640, 360), cv::Size(641, 361)}) {
        CAPTURE(size.width, size.height);
        const cv::Mat working = glared_side(size);
        const Contours expected = chroma_by_opencv(working);
        CHECK_FALSE(expected.empty());
        CHECK(cube::vision::chroma_contours(working) == expected);
    }
}

TEST_CASE("a frame that is a window into a larger one is read as a frame of its own",
          "[prepared_frame]") {
    // Rows of a window are not contiguous: a walk that assumed they were would
    // read the wrong pixels from the second row on.
    const cv::Mat whole = glared_side(cv::Size(700, 400));
    const cv::Rect window(30, 20, 640, 360);
    const cv::Mat view = whole(window);
    REQUIRE_FALSE(view.isContinuous());
    const cv::Mat own = view.clone();
    CHECK(cube::vision::contours_without_glare(view) == cube::vision::contours_without_glare(own));
    CHECK(cube::vision::chroma_contours(view) == cube::vision::chroma_contours(own));
}

TEST_CASE("the glare look of a prepared frame is the look of its working frame",
          "[prepared_frame]") {
    for (const cv::Size size : {cv::Size(640, 360), cv::Size(1280, 720), cv::Size(641, 361)}) {
        for (const bool with_glare : {true, false}) {
            CAPTURE(size.width, size.height, with_glare);
            cv::Mat frame = glared_side(size);
            if (!with_glare) {
                // Nothing clipped anywhere: the look has nothing to cut out, and
                // its outlines are the frame's own, handed back.
                cv::min(frame, cv::Scalar::all(200), frame);
            }
            for (const int cube_size : {3, 5}) {
                CAPTURE(cube_size);
                const std::optional<cube::vision::PreparedFrame> prepared =
                    cube::vision::prepare_frame(frame, cube_size);
                REQUIRE(prepared.has_value());
                const Contours expected =
                    cube::vision::contours_without_glare(prepared->working, cube_size);
                CHECK_FALSE(expected.empty());
                CHECK(cube::vision::contours_without_glare(*prepared, cube_size) == expected);
                if (!with_glare) {
                    // The shortcut is the frame's own outlines, not a second trace.
                    CHECK(expected == prepared->contours);
                }
                // A frame prepared for one size and looked at as another is
                // traced against the other's block.
                const int other = cube_size == 3 ? 5 : 3;
                CHECK(cube::vision::contours_without_glare(*prepared, other) ==
                      cube::vision::contours_without_glare(prepared->working, other));
            }
        }
    }
}
