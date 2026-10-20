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
    : QObject(parent),
      ids_withheld_until_use_(ids_withheld_until_use),
      enumerate_(std::move(enumerate)) {
    connect(&watcher_, &QFutureWatcher<Result<std::vector<platform::camera::DeviceInfo>>>::finished,
            this, &CameraController::handleEnumerationFinished);
}

QVariantList CameraController::devices() const {
    return devices_;
}

bool CameraController::busy() const {
    return watcher_.isRunning();
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
    if (watcher_.isRunning()) {
        return;
    }

    lastError_.clear();
    watcher_.setFuture(QtConcurrent::run(enumerate_));
    emit busyChanged();
}

void CameraController::handleEnumerationFinished() {
    const auto result = watcher_.result();
    emit busyChanged();

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
