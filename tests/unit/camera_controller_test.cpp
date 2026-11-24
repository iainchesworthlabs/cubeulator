// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "controllers/camera_controller.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStringList>
#include <QThreadPool>
#include <QVariantMap>

#include <chrono>
#include <condition_variable>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
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

// What a browser lists to a page that has not been allowed the camera: the cameras are only
// "the back one" and "the front one", with no id to open them by. And what it lists once a camera
// has been allowed, which is the list that has the real ids (see PlatformInfo).
const DeviceInfo kHiddenBack{
    .identifier = "facing:environment", .display_name = "Back camera", .facing = Facing::rear};
const DeviceInfo kHiddenFront{
    .identifier = "facing:user", .display_name = "Front camera", .facing = Facing::front};
const DeviceInfo kNamedBack{
    .identifier = "id-b", .display_name = "Back Camera", .facing = Facing::rear};
const DeviceInfo kNamedFront{
    .identifier = "id-a", .display_name = "Front Camera", .facing = Facing::front};

// The ids of a list as the controller gives it to QML, comma-separated, which Catch2 can print
// when a check on it fails (a QStringList it prints a character at a time).
std::string ids_of(const QVariantList& devices) {
    QStringList ids;
    for (const QVariant& device : devices) {
        ids.append(device.toMap()["id"].toString());
    }
    return ids.join(',').toStdString();
}

// A camera list that the test holds back. The worker waits inside the enumeration until release()
// (or the end of the test, which releases it too, so that a failed check cannot leave a worker
// waiting for ever), and so what the controller says while a probe is in flight is read from a
// probe that is certain to be in flight, not from one that may or may not have finished by the
// time the check runs. After release() the enumeration answers at once, which is what a second
// refresh finds.
//
// The probes are numbered in the order they begin, from 1. A test can let one of them go and hold
// the rest (release_call), and give one an answer of its own (answer_for): a list read before the
// person has allowed the camera is not the list read after.
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
            const int call = ++state->started;
            state->worker = std::this_thread::get_id();
            state->changed.notify_all();
            state->changed.wait(
                lock, [&] { return state->released || state->released_calls.contains(call); });
            if (const auto own = state->answers.find(call); own != state->answers.end()) {
                return own->second;
            }
            return state->answer;
        };
    }

    // What every probe answers, unless answer_for() has said otherwise for it.
    void answer(Answer next) {
        const std::scoped_lock lock(state_->mutex);
        state_->answer = std::move(next);
    }
    // What probe number `call` answers.
    void answer_for(int call, Answer next) {
        const std::scoped_lock lock(state_->mutex);
        state_->answers.insert_or_assign(call, std::move(next));
    }
    // Lets every probe go, those that have begun and those still to come.
    void release() {
        const std::scoped_lock lock(state_->mutex);
        state_->released = true;
        state_->changed.notify_all();
    }
    // Lets probe number `call` go and no other, so that the rest stay in flight.
    void release_call(int call) {
        const std::scoped_lock lock(state_->mutex);
        state_->released_calls.insert(call);
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
        std::set<int> released_calls;
        int started = 0;
        std::thread::id worker;
        Answer answer;
        std::map<int, Answer> answers;
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
    // Still no second probe beside the first, which has not been taken yet.
    CHECK(held.started() == 1);

    REQUIRE(wait_until([&] { return !controller.busy(); }));
    // The request is run once the first answer has been taken, one probe after the other: it is
    // kept, not dropped (see the cases below on what it is for).
    CHECK(held.started() == 2);
    CHECK(controller.devices().size() == 1);
    CHECK(busy_seen == std::vector<bool>{true, false});

    // With the list in, and nothing kept, the next refresh is a new probe.
    controller.refreshDevices();
    CHECK(controller.busy());
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(held.started() == 3);
}

TEST_CASE("CameraController reads the list again when it is asked to while a read is in flight",
          "[qt][controllers]") {
    // A browser hides the cameras' names until the person has allowed one, so the list read at
    // start is only "the back camera" and "the front camera". The camera is allowed while that
    // read is still going, and the request to read the list again, for the real one, arrives with
    // the first read in flight.
    HeldEnumeration held;
    held.answer_for(1, std::vector<DeviceInfo>{kHiddenBack, kHiddenFront});
    held.answer_for(2, std::vector<DeviceInfo>{kNamedBack, kNamedFront});
    CameraController controller(held.enumerator(), true);

    controller.refreshDevices();
    REQUIRE(held.wait_started(1));
    controller.refreshDevices();
    // Not a second probe beside the first: they would share the cameras, and a refresh in flight
    // is the only one the answer of which the controller waits for.
    CHECK(held.started() == 1);
    CHECK(controller.busy());

    held.release();
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    // The request was kept, and run when the first read ended: the list is the one read after the
    // camera was allowed, not the one read before.
    CHECK(held.started() == 2);
    CHECK(ids_of(controller.devices()) == "id-b,id-a");
    CHECK(controller.lastError().isEmpty());
    // The camera that was selected stood for "the back one", and is the real back camera now.
    CHECK(controller.selectedDeviceId().toStdString() == "id-b");
}

TEST_CASE("CameraController reads once more however often it is asked to during a read",
          "[qt][controllers]") {
    HeldEnumeration held({kBack});
    CameraController controller(held.enumerator(), false);
    std::vector<bool> busy_seen;
    QObject::connect(&controller, &CameraController::busyChanged,
                     [&] { busy_seen.push_back(controller.busy()); });

    controller.refreshDevices();
    REQUIRE(held.wait_started(1));
    for (int i = 0; i < 5; ++i) {
        controller.refreshDevices();
    }
    CHECK(held.started() == 1);

    held.release_call(1);
    // The one more read is in flight, and a request made now is for after it: it was asked for
    // after the first read began, and may be about what the second read began without.
    REQUIRE(wait_until([&] { return held.started() >= 2; }));
    controller.refreshDevices();
    controller.refreshDevices();
    CHECK(held.started() == 2);
    CHECK(controller.busy());

    held.release();
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(held.started() == 3);

    // Nothing is left over to start another: the reads were asked for, and are done.
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
    CHECK(held.started() == 3);
    CHECK_FALSE(controller.busy());
    // The controller said it was busy once, and idle once: it did not drop out of busy between
    // one read and the next, which a screen would show as the list having been settled.
    CHECK(busy_seen == std::vector<bool>{true, false});

    // And the request is not remembered once it has been run.
    controller.refreshDevices();
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(held.started() == 4);
}

TEST_CASE("CameraController reads the list again after a read that failed", "[qt][controllers]") {
    HeldEnumeration held;
    held.answer_for(1, std::unexpected(cube::CubeError::camera_not_found));
    held.answer_for(2, std::vector<DeviceInfo>{kBack});
    CameraController controller(held.enumerator(), false);
    QStringList errors;
    QObject::connect(&controller, &CameraController::errorOccurred,
                     [&](const QString& message) { errors.append(message); });

    controller.refreshDevices();
    REQUIRE(held.wait_started(1));
    controller.refreshDevices();
    held.release();
    REQUIRE(wait_until([&] { return !controller.busy(); }));

    // The first read failed, and said so; the one asked for after it began is not lost with it.
    CHECK(errors.size() == 1);
    CHECK(held.started() == 2);
    CHECK(ids_of(controller.devices()) == "back");
    CHECK(controller.lastError().isEmpty());
}

TEST_CASE("CameraController keeps the camera that was chosen when the list is read again",
          "[qt][controllers]") {
    HeldEnumeration held({kBack, kFront});
    CameraController controller(held.enumerator(), false);
    controller.refreshDevices();
    REQUIRE(held.wait_started(1));
    held.release();
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    REQUIRE(controller.selectedDeviceId() == QString("back"));
    controller.selectDevice("front");
    int selections = 0;
    QObject::connect(&controller, &CameraController::selectedDeviceIdChanged,
                     [&] { ++selections; });

    SECTION("the list is the same") {
        controller.refreshDevices();
        controller.refreshDevices();
        REQUIRE(wait_until([&] { return !controller.busy(); }));
        CHECK(held.started() == 3);
        CHECK(controller.selectedDeviceId().toStdString() == "front");
        CHECK(selections == 0);
    }

    SECTION("the camera is still there, the list is in another order") {
        held.answer(std::vector<DeviceInfo>{kFront, kBack});
        controller.refreshDevices();
        controller.refreshDevices();
        REQUIRE(wait_until([&] { return !controller.busy(); }));
        CHECK(held.started() == 3);
        CHECK(ids_of(controller.devices()) == "front,back");
        CHECK(controller.selectedDeviceId().toStdString() == "front");
        CHECK(selections == 0);
    }

    SECTION("no camera is found any more") {
        // Unplugged between the two reads: the list is empty, which is a list and not a failure,
        // and what was chosen is left for the person to notice and not swapped for another.
        held.answer(std::vector<DeviceInfo>{});
        QStringList errors;
        QObject::connect(&controller, &CameraController::errorOccurred,
                         [&](const QString& message) { errors.append(message); });
        controller.refreshDevices();
        controller.refreshDevices();
        REQUIRE(wait_until([&] { return !controller.busy(); }));
        CHECK(held.started() == 3);
        CHECK(controller.devices().isEmpty());
        CHECK(errors.isEmpty());
        CHECK(controller.selectedDeviceId().toStdString() == "front");
    }
}

namespace {

// How long a test's controller waits for a list before it gives up on it: the controller's own
// wait is a minute, which is not what a test can spend, and well past what a probe that has not
// stopped takes to answer here (it answers as soon as it is let go).
constexpr std::chrono::milliseconds kGiveUpAfter{600};

}  // namespace

TEST_CASE("CameraController gives up on a list that never arrives, and runs the read kept for it",
          "[qt][controllers]") {
    // Reading the cameras is a probe of drivers and hardware, and can stop for good. A controller
    // that waited for ever would take every request after it for one that is being read.
    HeldEnumeration held;
    held.answer_for(1, std::vector<DeviceInfo>{kBack});
    held.answer_for(2, std::vector<DeviceInfo>{kFront});
    held.release_call(2);
    CameraController controller(held.enumerator(), false, kGiveUpAfter);
    QStringList errors;
    QObject::connect(&controller, &CameraController::errorOccurred,
                     [&](const QString& message) { errors.append(message); });
    std::vector<bool> busy_seen;
    QObject::connect(&controller, &CameraController::busyChanged,
                     [&] { busy_seen.push_back(controller.busy()); });

    controller.refreshDevices();
    REQUIRE(held.wait_started(1));
    controller.refreshDevices();
    CHECK(held.started() == 1);

    // The first read is never let go. The second, which was kept, runs when it has been given up
    // on.
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(held.started() == 2);
    CHECK(ids_of(controller.devices()) == "front");
    // Giving up on the first was said, as a read that failed is; the second then cleared it.
    CHECK(errors.size() == 1);
    CHECK(controller.lastError().isEmpty());
    // Busy from the first request to the second read's answer, with no settled-looking gap between.
    CHECK(busy_seen == std::vector<bool>{true, false});

    // The probe that had stopped answers at last. It was given up on, and what it says is not the
    // list: the one that was read since stays.
    held.release();
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    CHECK(ids_of(controller.devices()) == "front");
    CHECK(errors.size() == 1);
    CHECK(held.started() == 2);
    CHECK_FALSE(controller.busy());
}

TEST_CASE("CameraController can read the list again after giving up on one that never arrived",
          "[qt][controllers]") {
    HeldEnumeration held;
    held.answer_for(1, std::vector<DeviceInfo>{kBack});
    held.answer_for(2, std::vector<DeviceInfo>{kFront});
    held.release_call(2);
    CameraController controller(held.enumerator(), false, kGiveUpAfter);
    QStringList errors;
    QObject::connect(&controller, &CameraController::errorOccurred,
                     [&](const QString& message) { errors.append(message); });

    controller.refreshDevices();
    REQUIRE(held.wait_started(1));
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(errors.size() == 1);
    CHECK(controller.lastError() == errors.first());
    CHECK_FALSE(controller.lastError().isEmpty());
    CHECK(controller.devices().isEmpty());

    // Nothing is read in its place: with nothing asked for, the controller does not look again on
    // its own, which with a driver that does not answer would be a probe after every give-up.
    CHECK_FALSE(wait_until([&] { return held.started() > 1; }, 1000));
    CHECK_FALSE(controller.busy());

    // The probe that stopped answers late, and what it says is not taken for a list.
    held.release_call(1);
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    CHECK(controller.devices().isEmpty());
    CHECK(errors.size() == 1);
    CHECK_FALSE(controller.busy());

    // And a request now is run, and clears the error, as it does after any other.
    controller.refreshDevices();
    CHECK(controller.busy());
    CHECK(controller.lastError().isEmpty());
    REQUIRE(wait_until([&] { return !controller.busy(); }));
    CHECK(held.started() == 2);
    CHECK(ids_of(controller.devices()) == "front");
    CHECK(errors.size() == 1);
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
