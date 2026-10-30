// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/detail/camera_frame.hpp"

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cmath>
#include <utility>

namespace cube::platform::camera::detail {

int quarter_turn_degrees(double degrees) noexcept {
    if (!std::isfinite(degrees)) {
        return 0;
    }
    const double turns = std::round(degrees / 90.0);
    const int quarter = static_cast<int>(std::fmod(turns, 4.0));
    return ((quarter % 4) + 4) % 4 * 90;
}

cv::Mat bgr_from_bgra(const BgraBuffer& buffer, double clockwise_degrees) {
    if (buffer.data == nullptr || buffer.width <= 0 || buffer.height <= 0) {
        return {};
    }

    // A header over the OS's pixels, no copy; cvtColor writes the copy. OpenCV's
    // step is a count of bytes, which is the stride. cv::Mat has no constructor
    // over read-only memory; the header is only ever read.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    const cv::Mat bgra(buffer.height, buffer.width, CV_8UC4, const_cast<std::uint8_t*>(buffer.data),
                       buffer.stride_bytes);
    cv::Mat bgr;
    cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);

    switch (quarter_turn_degrees(clockwise_degrees)) {
        case 90:
            cv::rotate(bgr, bgr, cv::ROTATE_90_CLOCKWISE);
            break;
        case 180:
            cv::rotate(bgr, bgr, cv::ROTATE_180);
            break;
        case 270:
            cv::rotate(bgr, bgr, cv::ROTATE_90_COUNTERCLOCKWISE);
            break;
        default:
            break;
    }
    return bgr;
}

void FrameMailbox::publish(cv::Mat frame) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const std::scoped_lock lock(mutex_);
    latest_ = TimedFrame{
        .frame = std::move(frame),
        .t_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count(),
    };
}

std::optional<TimedFrame> FrameMailbox::try_pop() {
    const std::scoped_lock lock(mutex_);
    return std::exchange(latest_, std::nullopt);
}

void FrameMailbox::clear() {
    const std::scoped_lock lock(mutex_);
    latest_.reset();
}

}  // namespace cube::platform::camera::detail
