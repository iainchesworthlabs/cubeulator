// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// Test cases only the web build can run: what the cameras a browser lists
// become in the list the app shows (platform/web/devices_web.hpp). Compiled
// into cube_integration_tests only for the web platform (tests/CMakeLists.txt
// picks tests/platform/<platform name>/) and run under Node with the rest of
// that suite.
//
// The point of the list is that a phone's first camera is the one facing away
// from the person: a cube is scanned with it, and the app opens the first one.
// What the page reads from the browser (camera_web.js) is the browser suite's
// (tests/web/specs/camera-device.spec.mjs); this is the half after it.
#include <catch2/catch_test_macros.hpp>

#include "interfaces/camera.hpp"
#include "platform/web/devices_web.hpp"

#include <string>
#include <utility>
#include <vector>

namespace {

using cube::platform::camera::DeviceInfo;
using cube::platform::camera::Facing;
using cube::platform::camera::web::device_list;
using cube::platform::camera::web::parse_cameras;
using cube::platform::camera::web::RawCamera;

// A camera as the page lists it. Not a designated initializer: the build is
// -Werror=missing-designated-field-initializers, and most of these name one or
// two of the three fields.
RawCamera cam(std::string id = "", std::string label = "", std::string facing = "") {
    return RawCamera{std::move(id), std::move(label), std::move(facing)};
}

std::vector<std::string> names(const std::vector<DeviceInfo>& devices) {
    std::vector<std::string> out;
    for (const DeviceInfo& device : devices) {
        out.push_back(device.display_name);
    }
    return out;
}

}  // namespace

TEST_CASE("a page's list of cameras is read three fields to a line", "[web][camera]") {
    const auto cameras = parse_cameras("id-1\tFront Camera\tuser\nid-2\tBack Camera\t\nid-3");

    REQUIRE(cameras.size() == 3);
    CHECK(cameras[0].id == "id-1");
    CHECK(cameras[0].label == "Front Camera");
    CHECK(cameras[0].facing == "user");
    CHECK(cameras[1].id == "id-2");
    CHECK(cameras[1].label == "Back Camera");
    CHECK(cameras[1].facing.empty());
    // A line with nothing after the id is a camera with neither name nor facing.
    CHECK(cameras[2].id == "id-3");
    CHECK(cameras[2].label.empty());
    CHECK(cameras[2].facing.empty());

    CHECK(parse_cameras("").empty());
}

TEST_CASE("the camera facing away is listed first, whatever order the browser lists them in",
          "[web][camera]") {
    SECTION("Safari lists the front camera first") {
        const auto devices = device_list({cam("a", "Front Camera"), cam("b", "Back Camera")});

        REQUIRE(devices.size() == 2);
        CHECK(devices[0].display_name == "Back Camera");
        CHECK(devices[0].facing == Facing::rear);
        CHECK(devices[1].display_name == "Front Camera");
        CHECK(devices[1].facing == Facing::front);
    }

    SECTION("Chrome's words for them") {
        const auto devices =
            device_list({cam("a", "camera2 1, facing front"), cam("b", "camera2 0, facing back")});

        REQUIRE(devices.size() == 2);
        CHECK(devices[0].identifier == "b");
        CHECK(devices[0].facing == Facing::rear);
        CHECK(devices[1].identifier == "a");
    }

    SECTION("the open camera's own track outranks its label") {
        const auto devices =
            device_list({cam("a", "Camera A"), cam("b", "Camera B", "environment")});

        REQUIRE(devices.size() == 2);
        CHECK(devices[0].identifier == "b");
        CHECK(devices[0].facing == Facing::rear);
        CHECK(devices[1].facing == Facing::unknown);
    }

    SECTION("cameras that say nothing keep the browser's order, ahead of a front one") {
        const auto devices = device_list(
            {cam("a", "Front Camera"), cam("b", "Capture card"), cam("c", "USB webcam")});

        CHECK(names(devices) ==
              std::vector<std::string>{"Capture card", "USB webcam", "Front Camera"});
    }

    SECTION("a laptop's one webcam is listed as it is, and is still mirrored") {
        const auto devices = device_list({cam("a", "Integrated Camera")});

        REQUIRE(devices.size() == 1);
        CHECK(devices[0].facing == Facing::unknown);
    }

    SECTION("a camera with no name is given a number") {
        const auto devices = device_list({cam("a"), cam("b")});

        CHECK(names(devices) == std::vector<std::string>{"Camera 1", "Camera 2"});
    }
}

TEST_CASE("before the browser says which cameras there are, the list is about direction",
          "[web][camera]") {
    SECTION("two cameras, nothing named: back first, then front") {
        const auto devices = device_list({cam(), cam()});

        REQUIRE(devices.size() == 2);
        CHECK(devices[0].identifier == "facing:environment");
        CHECK(devices[0].facing == Facing::rear);
        CHECK(devices[1].identifier == "facing:user");
        CHECK(devices[1].facing == Facing::front);
    }

    SECTION("one camera, or none said, is just a camera") {
        for (const auto& cameras : {std::vector<RawCamera>(1), std::vector<RawCamera>()}) {
            const auto devices = device_list(cameras);

            REQUIRE(devices.size() == 1);
            CHECK(devices[0].identifier == "default");
            CHECK(devices[0].facing == Facing::unknown);
        }
    }
}
