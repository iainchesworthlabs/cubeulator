// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "interfaces/camera.hpp"

#include <opencv2/core.hpp>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

namespace cube::platform::camera::detail {

// What a phone's camera API hands over in its capture callback: 8-bit pixels in
// B, G, R, A order (AVFoundation's kCVPixelFormatType_32BGRA) in a buffer that
// belongs to the OS and is only valid for the length of the callback. Rows may
// be padded, so the stride is carried instead of being worked out.
struct BgraBuffer {
    const std::uint8_t* data = nullptr;
    int width = 0;
    int height = 0;
    std::size_t stride_bytes = 0;
};

// Snaps an angle to the nearest of 0, 90, 180 and 270 (clockwise), whatever it
// was given: a rotation coordinator reports exact quarter turns, but a negative
// angle or one past a full turn must still mean something, and a stray fraction
// must not turn a frame into a skewed one.
[[nodiscard]] int quarter_turn_degrees(double degrees) noexcept;

// The buffer as an upright BGR image the scan pipeline can read, copied out of
// the OS's buffer. `clockwise_degrees` is how far the frame must be turned to
// be upright (the sensor of a phone sits in landscape, so a phone held upright
// needs 90). The frame is never mirrored here: the preview's mirror is display
// only (CameraController::previewMirrored), so the pipeline always reads the
// scene as it is. An empty buffer yields an empty image.
[[nodiscard]] cv::Mat bgr_from_bgra(const BgraBuffer& buffer, double clockwise_degrees);

// The "latest frame wins" slot between a native capture callback and the app's
// poll (interfaces/camera.hpp, try_pop_latest_frame): a frame that nobody took
// is replaced by the next, and a frame is handed out once. The stamp is taken
// at publish, which is delivery from the OS, as the interface's contract says.
class FrameMailbox {
public:
    void publish(cv::Mat frame);
    [[nodiscard]] std::optional<TimedFrame> try_pop();
    // Drops a frame nobody took, so that a camera that was stopped and started
    // again does not first deliver a picture of the last one's final moment.
    void clear();

private:
    std::mutex mutex_;
    std::optional<TimedFrame> latest_;
};

}  // namespace cube::platform::camera::detail
