// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "CameraPermission.hpp"

#include "interfaces/camera.hpp"

#include <QCoreApplication>
#include <QPermission>

CameraPermission::CameraPermission(QObject* parent) : QObject(parent) {
    refreshStatus();
}

int CameraPermission::status() const {
    return status_;
}

int CameraPermission::statusOf(Qt::PermissionStatus status) {
    switch (status) {
        case Qt::PermissionStatus::Granted:
            return kGranted;
        case Qt::PermissionStatus::Denied:
            return kDenied;
        case Qt::PermissionStatus::Undetermined:
            break;
    }
    return kUndetermined;
}

void CameraPermission::refreshStatus() {
    const auto new_status = statusOf(qApp->checkPermission(QCameraPermission{}));
    if (new_status != status_) {
        status_ = new_status;
        emit statusChanged();
    }
}

void CameraPermission::request() {
    qApp->requestPermission(QCameraPermission{}, this, [this](const QPermission& permission) {
        const auto new_status = statusOf(permission.status());
        if (new_status != status_) {
            status_ = new_status;
            emit statusChanged();
        }
    });
}

void CameraPermission::openSettings() {
    cube::platform::camera::open_permission_settings();
}
