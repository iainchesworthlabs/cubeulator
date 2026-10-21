#include <catch2/catch_test_macros.hpp>

#include "controllers/camera_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

using cube::app::CameraController;

namespace {

// enumerate_devices() runs on Qt's thread pool (see camera_controller.hpp),
// so tests need to pump the event loop until it finishes rather than
// reading results immediately after calling refreshDevices().
bool wait_until(const std::function<bool()>& predicate, int timeout_ms = 30000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeout_ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return predicate();
}

}  // namespace

TEST_CASE("CameraController starts idle with no devices", "[qt][controllers]") {
    CameraController controller;

    REQUIRE_FALSE(controller.busy());
    REQUIRE(controller.devices().isEmpty());
    REQUIRE(controller.selectedDeviceId().isEmpty());
}

TEST_CASE("CameraController::refreshDevices runs off the GUI thread and reports a result",
          "[qt][controllers]") {
    // No real camera is guaranteed on a CI runner; this only checks the
    // enumerate_devices()->QVariantList marshaling path doesn't throw and
    // leaves lastError empty, matching the platform contract that
    // enumeration always succeeds (possibly with an empty list).
    CameraController controller;

    controller.refreshDevices();
    REQUIRE(controller.busy());

    REQUIRE(wait_until([&] { return !controller.busy(); }));
    REQUIRE(controller.lastError().isEmpty());
}

TEST_CASE("CameraController::selectDevice updates the id and skips duplicate signals",
          "[qt][controllers]") {
    CameraController controller;
    int change_count = 0;
    QObject::connect(&controller, &CameraController::selectedDeviceIdChanged,
                     [&]() { ++change_count; });

    controller.selectDevice("cam-1");
    controller.selectDevice("cam-1");
    controller.selectDevice("cam-2");

    REQUIRE(controller.selectedDeviceId() == QString("cam-2"));
    REQUIRE(change_count == 2);
}

TEST_CASE("The preview is mirrored for every camera but a rear one", "[qt][controllers]") {
    using cube::platform::camera::Facing;

    // A front camera looks at the user, and a desktop webcam reports unknown
    // while doing the same: both are shown as a mirror shows them. A rear
    // camera shows the scene, and mirroring it would flip the cube's layout.
    REQUIRE(CameraController::previewMirroredFor(Facing::front));
    REQUIRE(CameraController::previewMirroredFor(Facing::unknown));
    REQUIRE_FALSE(CameraController::previewMirroredFor(Facing::rear));
}

TEST_CASE("CameraController mirrors the preview until a camera says otherwise",
          "[qt][controllers]") {
    // Nothing selected yet, which is how the scan screen first appears on a
    // desktop with a webcam: the default is the mirror.
    CameraController controller;

    REQUIRE(controller.previewMirrored());
}

namespace {

using cube::platform::camera::DeviceInfo;
using cube::platform::camera::Facing;

// A controller that lists what it is given, the first time and each time after:
// `devices` is read again at every refresh, so a test changes what the next
// one finds by changing it.
struct Listed {
    std::vector<DeviceInfo> devices;
    std::unique_ptr<CameraController> controller;

    explicit Listed(std::vector<DeviceInfo> first, bool ids_withheld_until_use = false)
        : devices(std::move(first)),
          controller(std::make_unique<CameraController>(
              [this]() -> cube::Result<std::vector<DeviceInfo>> { return devices; },
              ids_withheld_until_use)) {}

    // Until the controller has taken the answer: `busy()` goes false a moment
    // before its slot runs, so it is the list changing that is waited for.
    void refresh() {
        int lists = 0;
        const QMetaObject::Connection connection =
            QObject::connect(controller.get(), &CameraController::devicesChanged, [&] { ++lists; });
        controller->refreshDevices();
        REQUIRE(wait_until([&] { return lists > 0; }));
        QObject::disconnect(connection);
    }
};

const DeviceInfo kBack{.identifier = "back", .display_name = "Back", .facing = Facing::rear};
const DeviceInfo kFront{.identifier = "front", .display_name = "Front", .facing = Facing::front};

}  // namespace

TEST_CASE("CameraController::selectNextDevice steps through the cameras and wraps",
          "[qt][controllers]") {
    Listed listed({kBack, kFront});
    listed.refresh();
    // The first one listed is the one a first launch uses.
    REQUIRE(listed.controller->selectedDeviceId() == QString("back"));

    listed.controller->selectNextDevice();
    REQUIRE(listed.controller->selectedDeviceId() == QString("front"));
    // A selfie camera is mirrored, the back one is not.
    REQUIRE(listed.controller->previewMirrored());

    listed.controller->selectNextDevice();
    REQUIRE(listed.controller->selectedDeviceId() == QString("back"));
    REQUIRE_FALSE(listed.controller->previewMirrored());
}

TEST_CASE("CameraController::selectNextDevice does nothing with fewer than two cameras",
          "[qt][controllers]") {
    SECTION("none listed") {
        CameraController controller(
            []() -> cube::Result<std::vector<DeviceInfo>> { return std::vector<DeviceInfo>{}; },
            false);
        controller.selectNextDevice();
        REQUIRE(controller.selectedDeviceId().isEmpty());
    }
    SECTION("one listed") {
        Listed listed({kBack});
        listed.refresh();
        listed.controller->selectNextDevice();
        REQUIRE(listed.controller->selectedDeviceId() == QString("back"));
    }
}

TEST_CASE("a selection that stood for a direction becomes the camera facing that way",
          "[qt][controllers]") {
    // A browser hides its cameras until one is open: the list at launch is
    // "the back camera" and "the front camera", and the one after is the real
    // list, where those ids are gone.
    const DeviceInfo back_placeholder{
        .identifier = "facing:environment", .display_name = "Back camera", .facing = Facing::rear};
    const DeviceInfo front_placeholder{
        .identifier = "facing:user", .display_name = "Front camera", .facing = Facing::front};
    // Safari's order, which is the front one first.
    const DeviceInfo real_front{
        .identifier = "id-a", .display_name = "Front Camera", .facing = Facing::front};
    const DeviceInfo real_back{
        .identifier = "id-b", .display_name = "Back Camera", .facing = Facing::rear};

    SECTION("the back camera, picked at launch, is the real back camera") {
        Listed listed({back_placeholder, front_placeholder}, true);
        listed.refresh();
        REQUIRE(listed.controller->selectedDeviceId() == QString("facing:environment"));

        listed.devices = {real_back, real_front};
        listed.refresh();
        REQUIRE(listed.controller->selectedDeviceId() == QString("id-b"));
        REQUIRE_FALSE(listed.controller->previewMirrored());
    }

    SECTION("the front camera, chosen with the button, stays the front camera") {
        Listed listed({back_placeholder, front_placeholder}, true);
        listed.refresh();
        listed.controller->selectNextDevice();
        REQUIRE(listed.controller->selectedDeviceId() == QString("facing:user"));

        listed.devices = {real_back, real_front};
        listed.refresh();
        REQUIRE(listed.controller->selectedDeviceId() == QString("id-a"));
        REQUIRE(listed.controller->previewMirrored());
    }

    SECTION("a camera with no facing becomes the first one listed") {
        const DeviceInfo any{.identifier = "default", .display_name = "Camera"};
        Listed listed({any}, true);
        listed.refresh();
        REQUIRE(listed.controller->selectedDeviceId() == QString("default"));

        listed.devices = {real_back, real_front};
        listed.refresh();
        REQUIRE(listed.controller->selectedDeviceId() == QString("id-b"));
    }

    SECTION("a camera that is still listed is left alone") {
        Listed listed({real_back, real_front}, true);
        listed.refresh();
        listed.controller->selectDevice("id-a");

        listed.refresh();
        REQUIRE(listed.controller->selectedDeviceId() == QString("id-a"));
    }

    SECTION("where ids are not withheld a camera that has gone is not replaced") {
        // A desktop camera that is unplugged is for the person to notice, not
        // for the app to swap for another.
        Listed listed({real_back, real_front}, false);
        listed.refresh();
        listed.controller->selectDevice("id-b");

        listed.devices = {real_front};
        listed.refresh();
        REQUIRE(listed.controller->selectedDeviceId() == QString("id-b"));
    }
}
