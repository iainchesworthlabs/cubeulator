// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "controllers/camera_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThreadPool>

#include <chrono>
#include <condition_variable>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
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

using cube::platform::camera::DeviceInfo;
using cube::platform::camera::Facing;

const DeviceInfo kBack{.identifier = "back", .display_name = "Back", .facing = Facing::rear};
const DeviceInfo kFront{.identifier = "front", .display_name = "Front", .facing = Facing::front};

// A camera list that the test holds back. The worker waits inside the enumeration until release()
// (or the end of the test, which releases it too, so that a failed check cannot leave a worker
// waiting for ever), and so what the controller says while a probe is in flight is read from a
// probe that is certain to be in flight, not from one that may or may not have finished by the
// time the check runs. After release() the enumeration answers at once, which is what a second
// refresh finds.
class HeldEnumeration {
public:
    using Answer = cube::Result<std::vector<DeviceInfo>>;

    explicit HeldEnumeration(std::vector<DeviceInfo> devices = {})
        : state_(std::make_shared<State>()) {
        state_->answer = std::move(devices);
    }
    HeldEnumeration(const HeldEnumeration&) = delete;
    HeldEnumeration& operator=(const HeldEnumeration&) = delete;
    ~HeldEnumeration() { release(); }

    // What the controller is built with. Holds the state by shared pointer: the worker can outlive
    // this object, and must not find it gone.
    [[nodiscard]] CameraController::Enumerator enumerator() const {
        return [state = state_]() -> Answer {
            std::unique_lock lock(state->mutex);
            ++state->started;
            state->worker = std::this_thread::get_id();
            state->changed.notify_all();
            state->changed.wait(lock, [&] { return state->released; });
            return state->answer;
        };
    }

    void answer(Answer next) {
        const std::scoped_lock lock(state_->mutex);
        state_->answer = std::move(next);
    }
    void release() {
        const std::scoped_lock lock(state_->mutex);
        state_->released = true;
        state_->changed.notify_all();
    }
    // Until `count` enumerations have begun on a worker.
    [[nodiscard]] bool wait_started(int count) const {
        std::unique_lock lock(state_->mutex);
        return state_->changed.wait_for(lock, std::chrono::seconds(30),
                                        [&] { return state_->started >= count; });
    }
    [[nodiscard]] int started() const {
        const std::scoped_lock lock(state_->mutex);
        return state_->started;
    }
    [[nodiscard]] std::thread::id worker() const {
        const std::scoped_lock lock(state_->mutex);
        return state_->worker;
    }

private:
    struct State {
        mutable std::mutex mutex;
        std::condition_variable changed;
        bool released = false;
        int started = 0;
        std::thread::id worker;
        Answer answer;
    };
    std::shared_ptr<State> state_;
};

}  // namespace

TEST_CASE("CameraController starts idle with no devices", "[qt][controllers]") {
    CameraController controller;

    REQUIRE_FALSE(controller.busy());
    REQUIRE(controller.devices().isEmpty());
    REQUIRE(controller.selectedDeviceId().isEmpty());
}

TEST_CASE("CameraController::refreshDevices runs off the GUI thread and reports a result",
          "[qt][controllers]") {
    HeldEnumeration held({kBack, kFront});
    CameraController controller(held.enumerator(), false);
    int lists = 0;
    QObject::connect(&controller, &CameraController::devicesChanged, [&] { ++lists; });

    controller.refreshDevices();
    // The worker is waiting to be let go, so this is not a race with it: the controller is busy
    // because it has asked and not yet been told.
    REQUIRE(controller.busy());
    REQUIRE(held.wait_started(1));
    CHECK(held.worker() != std::this_thread::get_id());
    CHECK(controller.devices().isEmpty());

    held.release();
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(lists == 1);
    CHECK(controller.devices().size() == 2);
    CHECK(controller.lastError().isEmpty());
    // The first one listed is the one a first launch uses.
    CHECK(controller.selectedDeviceId() == QString("back"));
}

TEST_CASE("CameraController stays busy until it has the list, even when the probe has returned",
          "[qt][controllers]") {
    HeldEnumeration held({kBack});
    CameraController controller(held.enumerator(), false);
    std::vector<bool> busy_seen;
    QObject::connect(&controller, &CameraController::busyChanged,
                     [&] { busy_seen.push_back(controller.busy()); });

    controller.refreshDevices();
    REQUIRE(held.wait_started(1));
    held.release();
    // The probe has returned on its worker, but the event that says so has not been delivered:
    // the gap in which the future is finished, so the watcher no longer says it is running, and
    // the controller does not yet have the list. A refresh asked for in the gap found nothing in
    // flight and probed the cameras a second time, which replaced the future and with it the
    // answer of the first; and busy() said the list was settled when it was not.
    QThreadPool::globalInstance()->waitForDone();
    CHECK(controller.busy());
    CHECK(controller.devices().isEmpty());
    controller.refreshDevices();
    CHECK(held.started() == 1);

    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(held.started() == 1);
    CHECK(controller.devices().size() == 1);
    CHECK(busy_seen == std::vector<bool>{true, false});

    // With the list in, the next refresh is a new probe.
    controller.refreshDevices();
    CHECK(controller.busy());
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(held.started() == 2);
}

TEST_CASE("CameraController reports a list that could not be read, and can read it again",
          "[qt][controllers]") {
    HeldEnumeration held;
    held.answer(std::unexpected(cube::CubeError::camera_not_found));
    CameraController controller(held.enumerator(), false);
    QStringList errors;
    QObject::connect(&controller, &CameraController::errorOccurred,
                     [&](const QString& message) { errors.append(message); });

    controller.refreshDevices();
    held.release();
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    REQUIRE(errors.size() == 1);
    CHECK(controller.lastError() == errors.first());
    CHECK_FALSE(controller.lastError().isEmpty());
    CHECK(controller.devices().isEmpty());

    // A failure is not a probe that never ends: the next refresh runs, and clears the error.
    held.answer(std::vector<DeviceInfo>{kBack});
    controller.refreshDevices();
    CHECK(controller.lastError().isEmpty());
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(errors.size() == 1);
    CHECK(controller.lastError().isEmpty());
    CHECK(controller.devices().size() == 1);
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

    // Until the controller has taken the answer, which is the list changing.
    void refresh() {
        int lists = 0;
        const QMetaObject::Connection connection =
            QObject::connect(controller.get(), &CameraController::devicesChanged, [&] { ++lists; });
        controller->refreshDevices();
        REQUIRE(wait_until([&] { return lists > 0; }));
        QObject::disconnect(connection);
    }
};

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
