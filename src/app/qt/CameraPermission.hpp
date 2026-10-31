// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qqmlintegration.h>
#include <QObject>

// QML-facing camera permission status/request. Deliberately does not
// hand-roll per-platform permission code: Qt's own cross-platform
// QCameraPermission (QCoreApplication::checkPermission/requestPermission,
// since Qt 6.5) already covers checking and requesting on every platform
// with a runtime permission model, and returns Granted immediately on
// platforms without one (desktop). The one thing that API doesn't cover -
// opening the OS's app-settings page once a permission is durably denied -
// goes through cube::platform::camera::open_permission_settings(), which
// *is* real per-platform code (see interfaces/camera.hpp).
class CameraPermission : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // The answer as a plain int, so QML can compare without importing anything:
    // 0 undetermined, 1 denied, 2 granted. These are this class's numbers, not
    // Qt's: Qt::PermissionStatus counts Undetermined, Granted, Denied (0, 1, 2),
    // so the two disagree on 1 and 2 and the value is made by statusOf().
    Q_PROPERTY(int status READ status NOTIFY statusChanged)

public:
    explicit CameraPermission(QObject* parent = nullptr);

    static constexpr int kUndetermined = 0;
    static constexpr int kDenied = 1;
    static constexpr int kGranted = 2;

    // Qt's answer as the value `status` has for it.
    [[nodiscard]] static int statusOf(Qt::PermissionStatus status);

    [[nodiscard]] int status() const;

    // Triggers the OS permission prompt if the status is undetermined (or
    // re-prompts if still allowed to); status updates + statusChanged fires
    // once the user responds. Safe to call when already granted/denied.
    Q_INVOKABLE void request();

    // Escape hatch for "denied and the system won't prompt again".
    Q_INVOKABLE void openSettings();

signals:
    void statusChanged();

private:
    void refreshStatus();

    int status_ = 0;
};
