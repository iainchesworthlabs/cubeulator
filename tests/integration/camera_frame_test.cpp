// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// The frame adapter between a phone's camera callback and the scan pipeline
// (platform/detail/camera_frame.hpp). It is plain C++ and runs on every
// platform, so that what an iPhone does to a frame - the channel order, the
// padding of a row, the turn that makes a landscape sensor upright - is
// checked against drawn frames in CI, where no phone is.

#include <catch2/catch_test_macros.hpp>

#include "../unit/must_have.hpp"
#include "platform/detail/camera_frame.hpp"

#include <opencv2/core.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

using cube::platform::camera::detail::bgr_from_bgra;
using cube::platform::camera::detail::BgraBuffer;
using cube::platform::camera::detail::FrameMailbox;
using cube::platform::camera::detail::quarter_turn_degrees;
using cube::test::must_have;

namespace {

constexpr std::size_t kStride = 32;  // 4 pixels of 4 bytes, and 16 bytes of padding
constexpr std::size_t kRows = 2;

struct Colour {
    std::uint8_t b = 0;
    std::uint8_t g = 0;
    std::uint8_t r = 0;
};

// A 4 x 2 landscape frame, rows padded the way a camera buffer is. The top-left
// pixel is red, the top-right green, the bottom-left blue and the bottom-right
// white, in B, G, R, A; the padding is deliberately not zero.
struct Drawn {
    std::array<std::uint8_t, kStride * kRows> bytes{};

    [[nodiscard]] BgraBuffer buffer() const {
        return BgraBuffer{.data = bytes.data(), .width = 4, .height = 2, .stride_bytes = kStride};
    }

    void put(std::size_t row, std::size_t column, Colour colour) {
        const std::size_t at = (row * kStride) + (column * 4);
        bytes.at(at) = colour.b;
        bytes.at(at + 1) = colour.g;
        bytes.at(at + 2) = colour.r;
        bytes.at(at + 3) = 255;
    }
};

Drawn draw_landscape() {
    Drawn drawn;
    drawn.bytes.fill(0xEE);
    for (std::size_t row = 0; row < kRows; ++row) {
        for (std::size_t column = 0; column < 4; ++column) {
            drawn.put(row, column, Colour{});
        }
    }
    drawn.put(0, 0, Colour{.b = 0, .g = 0, .r = 255});      // red
    drawn.put(0, 3, Colour{.b = 0, .g = 255, .r = 0});      // green
    drawn.put(1, 0, Colour{.b = 255, .g = 0, .r = 0});      // blue
    drawn.put(1, 3, Colour{.b = 255, .g = 255, .r = 255});  // white
    return drawn;
}

cv::Vec3b at(const cv::Mat& image, int row, int column) {
    return image.at<cv::Vec3b>(row, column);
}

}  // namespace

TEST_CASE("The channels come out as B, G, R and the row padding is ignored", "[camera][frame]") {
    const Drawn drawn = draw_landscape();
    const cv::Mat bgr = bgr_from_bgra(drawn.buffer(), 0.0);

    REQUIRE(bgr.rows == 2);
    REQUIRE(bgr.cols == 4);
    REQUIRE(bgr.type() == CV_8UC3);
    CHECK(at(bgr, 0, 0) == cv::Vec3b(0, 0, 255));
    CHECK(at(bgr, 0, 3) == cv::Vec3b(0, 255, 0));
    CHECK(at(bgr, 1, 0) == cv::Vec3b(255, 0, 0));
    CHECK(at(bgr, 1, 3) == cv::Vec3b(255, 255, 255));
    CHECK(at(bgr, 0, 1) == cv::Vec3b(0, 0, 0));
}

TEST_CASE("The result owns its pixels", "[camera][frame]") {
    Drawn drawn = draw_landscape();
    const cv::Mat bgr = bgr_from_bgra(drawn.buffer(), 0.0);
    drawn.bytes.fill(0);  // the OS reuses its buffer
    CHECK(at(bgr, 0, 0) == cv::Vec3b(0, 0, 255));
}

TEST_CASE("A quarter turn clockwise puts the top-left corner top-right", "[camera][frame]") {
    const Drawn drawn = draw_landscape();
    const cv::Mat turned = bgr_from_bgra(drawn.buffer(), 90.0);

    REQUIRE(turned.rows == 4);
    REQUIRE(turned.cols == 2);
    CHECK(at(turned, 0, 1) == cv::Vec3b(0, 0, 255));      // red: top-left -> top-right
    CHECK(at(turned, 3, 1) == cv::Vec3b(0, 255, 0));      // green: top-right -> bottom-right
    CHECK(at(turned, 0, 0) == cv::Vec3b(255, 0, 0));      // blue: bottom-left -> top-left
    CHECK(at(turned, 3, 0) == cv::Vec3b(255, 255, 255));  // white: bottom-right -> bottom-left
}

TEST_CASE("Half a turn and three quarters", "[camera][frame]") {
    const Drawn drawn = draw_landscape();

    const cv::Mat half = bgr_from_bgra(drawn.buffer(), 180.0);
    REQUIRE(half.rows == 2);
    REQUIRE(half.cols == 4);
    CHECK(at(half, 1, 3) == cv::Vec3b(0, 0, 255));  // red: top-left -> bottom-right
    CHECK(at(half, 0, 0) == cv::Vec3b(255, 255, 255));

    const cv::Mat three = bgr_from_bgra(drawn.buffer(), 270.0);
    REQUIRE(three.rows == 4);
    REQUIRE(three.cols == 2);
    CHECK(at(three, 3, 0) == cv::Vec3b(0, 0, 255));  // red: top-left -> bottom-left
    CHECK(at(three, 0, 0) == cv::Vec3b(0, 255, 0));  // green: top-right -> top-left
}

TEST_CASE("The frame is never mirrored", "[camera][frame]") {
    // The preview's mirror is display only: the same frame comes out for a
    // front and a rear camera, so the pipeline always reads the cube as it is.
    const Drawn drawn = draw_landscape();
    const cv::Mat bgr = bgr_from_bgra(drawn.buffer(), 0.0);
    CHECK(at(bgr, 0, 0) == cv::Vec3b(0, 0, 255));
    CHECK(at(bgr, 0, 3) == cv::Vec3b(0, 255, 0));
}

TEST_CASE("Angles snap to a quarter turn whatever they are given", "[camera][frame]") {
    CHECK(quarter_turn_degrees(0.0) == 0);
    CHECK(quarter_turn_degrees(90.0) == 90);
    CHECK(quarter_turn_degrees(180.0) == 180);
    CHECK(quarter_turn_degrees(270.0) == 270);
    CHECK(quarter_turn_degrees(360.0) == 0);
    CHECK(quarter_turn_degrees(450.0) == 90);
    CHECK(quarter_turn_degrees(-90.0) == 270);
    CHECK(quarter_turn_degrees(-180.0) == 180);
    CHECK(quarter_turn_degrees(89.0) == 90);
    CHECK(quarter_turn_degrees(44.0) == 0);
    CHECK(quarter_turn_degrees(std::numeric_limits<double>::quiet_NaN()) == 0);
    CHECK(quarter_turn_degrees(std::numeric_limits<double>::infinity()) == 0);
}

TEST_CASE("A buffer with nothing in it gives no image", "[camera][frame]") {
    CHECK(bgr_from_bgra(BgraBuffer{}, 90.0).empty());
    const std::array<std::uint8_t, 4> pixel{1, 2, 3, 4};
    CHECK(bgr_from_bgra(
              BgraBuffer{.data = pixel.data(), .width = 0, .height = 1, .stride_bytes = 4}, 0.0)
              .empty());
}

TEST_CASE("The mailbox hands a frame out once, newest first", "[camera][frame]") {
    FrameMailbox mailbox;
    CHECK_FALSE(mailbox.try_pop().has_value());

    mailbox.publish(cv::Mat(2, 2, CV_8UC3, cv::Scalar(1, 1, 1)));
    mailbox.publish(cv::Mat(3, 3, CV_8UC3, cv::Scalar(2, 2, 2)));  // replaces the first

    const auto popped = mailbox.try_pop();
    const auto& frame = must_have(popped);
    CHECK(frame.frame.rows == 3);
    CHECK(frame.t_ms > 0);
    CHECK_FALSE(mailbox.try_pop().has_value());
}

TEST_CASE("The mailbox stamps frames in order and clears", "[camera][frame]") {
    FrameMailbox mailbox;
    mailbox.publish(cv::Mat(1, 1, CV_8UC3));
    const auto first = mailbox.try_pop();
    mailbox.publish(cv::Mat(1, 1, CV_8UC3));
    const auto second = mailbox.try_pop();
    CHECK(must_have(second).t_ms >= must_have(first).t_ms);

    mailbox.publish(cv::Mat(1, 1, CV_8UC3));
    mailbox.clear();
    CHECK_FALSE(mailbox.try_pop().has_value());
}
