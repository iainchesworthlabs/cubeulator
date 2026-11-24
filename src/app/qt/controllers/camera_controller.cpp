// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "controllers/camera_controller.hpp"

#include "interfaces/platform_info.hpp"

#include <QVariantMap>
#include <QtConcurrent/QtConcurrentRun>

#include <utility>

namespace cube::app {

CameraController::CameraController(QObject* parent)
    : CameraController(&platform::camera::enumerate_devices,
                       platform::platform_info::current().camera_ids_withheld_until_use, parent) {}

CameraController::CameraController(Enumerator enumerate, bool ids_withheld_until_use,
                                   QObject* parent)
    : CameraController(std::move(enumerate), ids_withheld_until_use, kRefreshTimeout, parent) {}

CameraController::CameraController(Enumerator enumerate, bool ids_withheld_until_use,
                                   std::chrono::milliseconds refresh_timeout, QObject* parent)
    : QObject(parent),
      ids_withheld_until_use_(ids_withheld_until_use),
      enumerate_(std::move(enumerate)) {
    timeout_.setSingleShot(true);
    timeout_.setInterval(refresh_timeout);
    connect(&timeout_, &QTimer::timeout, this, &CameraController::handleRefreshTimedOut);
}

QVariantList CameraController::devices() const {
    return devices_;
}

bool CameraController::busy() const {
    return busy_;
}

QString CameraController::selectedDeviceId() const {
    return selectedDeviceId_;
}

QString CameraController::lastError() const {
    return lastError_;
}

bool CameraController::previewMirroredFor(platform::camera::Facing facing) noexcept {
    // Selfie-mirror for front and unknown facings; rear previews show the
    // scene rather than the user and must stay un-mirrored (see Facing in
    // interfaces/camera.hpp).
    return facing != platform::camera::Facing::rear;
}

bool CameraController::previewMirrored() const {
    // A selection with no matching entry (nothing selected yet, or a stale id
    // after re-enumeration) is treated as unknown, which keeps the
    // desktop-webcam default of mirroring.
    const std::string id = selectedDeviceId_.toStdString();
    for (const auto& device : deviceInfos_) {
        if (device.identifier == id) {
            return previewMirroredFor(device.facing);
        }
    }
    return previewMirroredFor(platform::camera::Facing::unknown);
}

void CameraController::refreshDevices() {
    if (busy_) {
        // A list is being read. Not a second probe beside it, but not dropped either: the request
        // can be about something the read in flight began without (a browser gives the cameras'
        // names only once the camera has been allowed, and that can happen while the list is
        // being read), so it is run when that read ends. Once, whatever number of requests came.
        refreshQueued_ = true;
        return;
    }

    lastError_.clear();
    busy_ = true;
    startProbe();
    emit busyChanged();
}

void CameraController::startProbe() {
    probe_ = new Probe(this);
    connect(probe_, &Probe::finished, this, &CameraController::handleEnumerationFinished);
    probe_->setFuture(QtConcurrent::run(enumerate_));
    timeout_.start();
}

bool CameraController::endProbe() {
    timeout_.stop();
    // Deleted, not kept for the next read: a watcher that is gone reports nothing more, so the
    // answer of a read that was given up on, if it ever comes, finds nobody to take it. The
    // worker finishes into the future it was given, which nothing is waiting on.
    probe_->disconnect(this);
    probe_->deleteLater();
    probe_ = nullptr;

    // With a request waiting, the controller stays busy across the hand-over: it has not got the
    // list that was asked for, and a screen that took a false here for "settled" would show a list
    // that is about to change.
    const bool again = std::exchange(refreshQueued_, false);
    if (!again) {
        busy_ = false;
        emit busyChanged();
    }
    return again;
}

void CameraController::handleEnumerationFinished() {
    const Answer result = probe_->result();
    const bool again = endProbe();

    takeAnswer(result);

    if (again) {
        lastError_.clear();
        startProbe();
    }
}

void CameraController::handleRefreshTimedOut() {
    if (probe_ == nullptr) {
        return;
    }
    const bool again = endProbe();

    // As a read that failed: a later request is a new read, and clears this. A code and not
    // wording, as a controller gives one (the i18n lint holds to it); nothing shows lastError().
    lastError_ = QStringLiteral("timedOut");
    emit errorOccurred(lastError_);

    if (again) {
        lastError_.clear();
        startProbe();
    }
}

void CameraController::takeAnswer(const Answer& result) {
    if (!result.has_value()) {
        lastError_ = QString::fromStdString(std::string(to_string(result.error())));
        emit errorOccurred(lastError_);
        return;
    }

    const bool wasMirrored = previewMirrored();
    const std::optional<platform::camera::Facing> selectedFacing =
        facingOf(selectedDeviceId_.toStdString());

    deviceInfos_ = *result;
    devices_.clear();
    for (const auto& device : deviceInfos_) {
        QVariantMap entry;
        entry["id"] = QString::fromStdString(device.identifier);
        entry["label"] = QString::fromStdString(device.display_name);
        devices_.append(entry);
    }
    emit devicesChanged();

    if (selectedDeviceId_.isEmpty() && !devices_.isEmpty()) {
        selectDevice(devices_.first().toMap()["id"].toString());
    } else if (ids_withheld_until_use_ && !devices_.isEmpty() &&
               !facingOf(selectedDeviceId_.toStdString()).has_value()) {
        // What was selected stood for a direction ("the rear camera") and the
        // list now names the cameras. Keep the one facing the same way, which
        // is the one that was opened; failing that the first, which is the one
        // facing away from the person where there is one.
        const platform::camera::DeviceInfo* match = &deviceInfos_.front();
        if (selectedFacing.has_value() && *selectedFacing != platform::camera::Facing::unknown) {
            for (const auto& device : deviceInfos_) {
                if (device.facing == *selectedFacing) {
                    match = &device;
                    break;
                }
            }
        }
        selectDevice(QString::fromStdString(match->identifier));
    } else if (previewMirrored() != wasMirrored) {
        // previewMirrored's NOTIFY is selectedDeviceIdChanged (see header):
        // a refresh can change the facing recorded for an unchanged
        // selection, and without this re-emit QML bindings on
        // previewMirrored would keep the stale value.
        emit selectedDeviceIdChanged();
    }
}

std::optional<platform::camera::Facing> CameraController::facingOf(
    const std::string& deviceId) const {
    for (const auto& device : deviceInfos_) {
        if (device.identifier == deviceId) {
            return device.facing;
        }
    }
    return std::nullopt;
}

void CameraController::selectNextDevice() {
    if (deviceInfos_.size() < 2) {
        return;
    }
    const std::string current = selectedDeviceId_.toStdString();
    std::size_t next = 0;
    for (std::size_t i = 0; i < deviceInfos_.size(); ++i) {
        if (deviceInfos_[i].identifier == current) {
            next = (i + 1) % deviceInfos_.size();
            break;
        }
    }
    selectDevice(QString::fromStdString(deviceInfos_[next].identifier));
}

void CameraController::selectDevice(const QString& deviceId) {
    if (selectedDeviceId_ == deviceId) {
        return;
    }
    selectedDeviceId_ = deviceId;
    emit selectedDeviceIdChanged();
}

}  // namespace cube::app
