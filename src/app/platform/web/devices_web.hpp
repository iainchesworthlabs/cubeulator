// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "interfaces/camera.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The list of cameras the browser gave, as the list the app shows.
//
// camera_web.js reads navigator.mediaDevices.enumerateDevices() and hands it
// over as text, one `deviceId<TAB>label<TAB>facing` line per camera (the last
// field is empty unless the page knows which way the lens points, which it does
// only for the camera it has open). What the app should make of it is decided
// here, kept apart from camera_web.cpp (which calls the page and so cannot be
// linked into a test) so that it is tested under Node with the rest of the web
// build's cases (tests/platform/web).
namespace cube::platform::camera::web {

// Camera device ids the page cannot supply, because it has not been given
// camera access yet (browsers hide the real ids and labels until then). They
// are passed back to camera_web.js as written, which turns each into a request
// for a camera facing that way rather than for one by name.
inline constexpr std::string_view kDefaultId = "default";
inline constexpr std::string_view kBackFacingId = "facing:environment";
inline constexpr std::string_view kFrontFacingId = "facing:user";

struct RawCamera {
    std::string id;
    std::string label;
    // "environment" or "user" as the browser's own track says, or empty.
    std::string facing;
};

[[nodiscard]] inline std::vector<RawCamera> parse_cameras(std::string_view text) {
    std::vector<RawCamera> cameras;
    while (!text.empty()) {
        const std::size_t end = std::min(text.find('\n'), text.size());
        std::string_view line = text.substr(0, end);
        RawCamera camera;
        const auto take_field = [&line]() {
            const std::size_t tab = std::min(line.find('\t'), line.size());
            std::string field(line.substr(0, tab));
            line.remove_prefix(std::min(tab + 1, line.size()));
            return field;
        };
        camera.id = take_field();
        camera.label = take_field();
        camera.facing = take_field();
        cameras.push_back(std::move(camera));
        text.remove_prefix(std::min(end + 1, text.size()));
    }
    return cameras;
}

[[nodiscard]] inline bool contains_word(const std::string& haystack, std::string_view word) {
    std::string lower = haystack;
    std::ranges::transform(lower, lower.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower.find(word) != std::string::npos;
}

// Which way a lens points, from what the browser says of it: the track's own
// facingMode where the page has one open, else the words in the label. Chrome
// on a phone words them "camera2 0, facing back" and "camera2 1, facing front";
// Safari "Back Camera" and "Front Camera"; desktop webcams say nothing and stay
// unknown, which gets the selfie mirror they want (see Facing in
// interfaces/camera.hpp).
[[nodiscard]] inline Facing facing_of(const RawCamera& camera) {
    if (camera.facing == "environment") {
        return Facing::rear;
    }
    if (camera.facing == "user") {
        return Facing::front;
    }
    const std::string& label = camera.label;
    if (contains_word(label, "back") || contains_word(label, "rear") ||
        contains_word(label, "environment")) {
        return Facing::rear;
    }
    if (contains_word(label, "front") || contains_word(label, "user") ||
        contains_word(label, "face")) {
        return Facing::front;
    }
    return Facing::unknown;
}

// The list the app shows. The first entry is the camera a first launch uses, so
// the cameras facing away from the person come first: a phone's front camera
// looks at the person, not at the cube. (Browsers list in their own order:
// Safari puts the front camera first.) Cameras that say nothing about their
// facing follow, and front ones are last; order is otherwise the browser's.
//
// Until a person has allowed the camera for this site the browser hides every
// id and label, so a device cannot be picked out by name; what can be said then
// is how many there are, and - the one thing a phone's two cameras differ in,
// and the one a cube is scanned with - which way they point.
[[nodiscard]] inline std::vector<DeviceInfo> device_list(const std::vector<RawCamera>& cameras) {
    const bool identified =
        std::ranges::any_of(cameras, [](const RawCamera& camera) { return !camera.id.empty(); });

    std::vector<DeviceInfo> devices;
    if (identified) {
        int number = 0;
        for (const RawCamera& camera : cameras) {
            ++number;
            devices.push_back(DeviceInfo{
                .identifier = camera.id,
                .display_name =
                    camera.label.empty() ? "Camera " + std::to_string(number) : camera.label,
                .facing = facing_of(camera),
            });
        }
        const auto rank = [](const DeviceInfo& device) {
            switch (device.facing) {
                case Facing::rear:
                    return 0;
                case Facing::unknown:
                    return 1;
                case Facing::front:
                    return 2;
            }
            return 1;
        };
        std::ranges::stable_sort(
            devices, [&](const DeviceInfo& a, const DeviceInfo& b) { return rank(a) < rank(b); });
    } else if (cameras.size() >= 2) {
        devices.push_back(DeviceInfo{.identifier = std::string(kBackFacingId),
                                     .display_name = "Back camera",
                                     .facing = Facing::rear});
        devices.push_back(DeviceInfo{.identifier = std::string(kFrontFacingId),
                                     .display_name = "Front camera",
                                     .facing = Facing::front});
    } else {
        // One camera, or the browser would not even say how many. Either way
        // asking for "a camera" is right: it opens the only one, and where
        // there are several camera_web.js asks for the one facing away.
        devices.push_back(DeviceInfo{.identifier = std::string(kDefaultId),
                                     .display_name = "Camera",
                                     .facing = Facing::unknown});
    }
    return devices;
}

}  // namespace cube::platform::camera::web
