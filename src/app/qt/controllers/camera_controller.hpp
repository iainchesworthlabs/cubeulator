// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "interfaces/camera.hpp"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace cube::app {

// Owns cube::platform::camera device enumeration/selection state for the
// QML shell. Deliberately does not itself start/stop a capture session -
// that's DirectScan's job - nor does it track OS camera-permission status,
// which CameraPermission (a thin QCameraPermission wrapper) owns instead;
// this only tracks which devices exist and which one is selected.
//
// enumerate_devices() is a real blocking hardware probe (observed 15-20s on
// this machine, opening/closing several device indices) - it must never run
// synchronously on the calling (GUI) thread, or the whole window freezes for
// that long. It runs via QtConcurrent on Qt's thread pool instead; callers
// react to devicesChanged/errorOccurred rather than reading the result
// immediately after calling refreshDevices().
//
// One list is read at a time, and a request made while it is being read is kept,
// not dropped: see refreshDevices(). A read that does not answer is given up on after
// kRefreshTimeout, so that it cannot hold every later request for ever.
class CameraController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString selectedDeviceId READ selectedDeviceId NOTIFY selectedDeviceIdChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    // Derived from the selected device's facing: true (selfie-mirror) for
    // front and unknown, false for rear - see Facing in interfaces/camera.hpp
    // for why rear previews must stay un-mirrored. NOTIFY rides
    // selectedDeviceIdChanged because selection is what changes the value;
    // handleEnumerationFinished re-emits that signal when a refresh flips
    // the facing recorded for an unchanged selection.
    Q_PROPERTY(bool previewMirrored READ previewMirrored NOTIFY selectedDeviceIdChanged)

public:
    // How the cameras are listed: the platform's enumerate_devices() in the app.
    using Enumerator = std::function<Result<std::vector<platform::camera::DeviceInfo>>()>;

    // How long a read of the list may take before the controller gives up on it. A probe of the
    // cameras has been seen to take 15-20 s on a machine with several, and the browser's own wait
    // (camera_web.cpp) is 15 s; a minute is well past either, and short enough that a probe that
    // has stopped for good (a driver that never answers) does not leave Settings saying "Looking
    // for cameras" until the app is closed.
    static constexpr std::chrono::milliseconds kRefreshTimeout{60'000};

    explicit CameraController(QObject* parent = nullptr);
    // For a test, which has no cameras of its own to list. `ids_withheld_until_use`
    // is PlatformInfo::camera_ids_withheld_until_use.
    CameraController(Enumerator enumerate, bool ids_withheld_until_use, QObject* parent = nullptr);
    // The same, with the wait for a list that a test can shorten.
    CameraController(Enumerator enumerate, bool ids_withheld_until_use,
                     std::chrono::milliseconds refresh_timeout, QObject* parent = nullptr);

    [[nodiscard]] QVariantList devices() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QString selectedDeviceId() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] bool previewMirrored() const;

    // The rule previewMirrored() applies to one device's facing, on its own
    // so it can be checked without a camera. The mirror is how the picture is
    // painted and nothing else: the frames that are read stay as the camera
    // delivered them (see FrameImageProvider, DirectScan::latestFrame).
    [[nodiscard]] static bool previewMirroredFor(platform::camera::Facing facing) noexcept;

public slots:
    // Reads the list of cameras again, off the GUI thread. With a read already in flight it starts
    // no second one beside it (two probes of the cameras at once would race for them, and the
    // controller waits for the answer of one): the request is kept, and when that read ends
    // exactly one more runs, however many requests came meanwhile. The request is not for the
    // read already going, which can be a list made stale by what prompted the request: a
    // browser gives the cameras' names only once the camera has been allowed (see
    // PlatformInfo::camera_ids_withheld_until_use), and the request to read again arrives from
    // that, which may be while the first list is being read. busy() stays true from the first
    // request until the last of these reads has been taken. The camera that is selected is
    // carried over to the new list (see takeAnswer() in the .cpp).
    //
    // A read that has not answered in kRefreshTimeout is given up on: errorOccurred() says so (as
    // the code "timedOut"), its answer is dropped if it ever comes, and the request that was kept
    // for it, if there is one, runs as a read of its own (itself given up on if it too does not
    // answer). Nothing is read again unasked, so a probe that never answers costs one thread of the
    // pool for as long as it is stuck, and not a probe a minute.
    void refreshDevices();
    void selectDevice(const QString& deviceId);
    // Selects the next camera in the list, going back to the first after the
    // last. The scan screen's "switch camera" button; a no-op with fewer than
    // two cameras. Does not restart a capture - the caller starts the scan
    // again with the new selection, as DirectScan::start takes the id.
    void selectNextDevice();

signals:
    void devicesChanged();
    void busyChanged();
    void selectedDeviceIdChanged();
    void errorOccurred(const QString& message);

private slots:
    void handleEnumerationFinished();
    void handleRefreshTimedOut();

private:
    using Answer = Result<std::vector<platform::camera::DeviceInfo>>;
    using Probe = QFutureWatcher<Answer>;

    // Starts reading the list on the thread pool, and the wait for it. Only with none in flight.
    void startProbe();
    // The read in flight is over, answered or given up on: lets go of it and, unless a request
    // was kept for another, says the controller is no longer busy. Returns whether another read
    // is to follow, which the caller starts once it has dealt with what the first said.
    [[nodiscard]] bool endProbe();
    // Makes `answer` the list, or the error, and settles the selection against it.
    void takeAnswer(const Answer& answer);

    // The facing recorded for `deviceId` in the list last read, if it is there.
    [[nodiscard]] std::optional<platform::camera::Facing> facingOf(
        const std::string& deviceId) const;

private:
    QVariantList devices_;
    // Keeps the platform-level enumeration result (facing included) beside
    // the QML-facing QVariantList so previewMirrored() can resolve the
    // selected id without round-tripping through QVariant.
    std::vector<platform::camera::DeviceInfo> deviceInfos_;
    QString selectedDeviceId_;
    QString lastError_;
    // See PlatformInfo::camera_ids_withheld_until_use: a selection the list no
    // longer holds is then a placeholder that has become a device.
    bool ids_withheld_until_use_ = false;
    Enumerator enumerate_;
    // The read in flight, if there is one: a child of this, made for that read alone, and
    // deleted once its answer is taken. A watcher of its own for each, and not one that is given
    // each future in turn, so that what it reports can only be about the read it was made for.
    Probe* probe_ = nullptr;
    // The wait for probe_: single shot, started with it, stopped when it answers.
    QTimer timeout_;
    // A list is being read: from refreshDevices() until handleEnumerationFinished() has taken the
    // answer. Not the watcher's isRunning(), which turns false when the worker returns, a queued
    // event before handleEnumerationFinished() runs. A refreshDevices() in that gap saw nothing
    // running and started a second probe of the cameras, which replaced the future and so dropped
    // the answer of the first, and busy() said the list was settled when it was not yet.
    bool busy_ = false;
    // refreshDevices() was called while busy_, and the list has not been read again since: one
    // more read follows the one in flight.
    bool refreshQueued_ = false;
};

}  // namespace cube::app
