// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include "CameraPermission.hpp"

#include <QtCore/qnamespace.h>

// The screens compare CameraPermission.status with 1 for "denied" and 2 for "granted". Qt's own
// enum counts Undetermined, Granted, Denied (0, 1, 2), so passing its value through as it was
// once made an iPhone that had just allowed the camera read as "Camera access is off".
TEST_CASE("the camera permission status has the numbers the screens compare with",
          "[qml][camera-permission]") {
    CHECK(CameraPermission::statusOf(Qt::PermissionStatus::Undetermined) == 0);
    CHECK(CameraPermission::statusOf(Qt::PermissionStatus::Denied) == 1);
    CHECK(CameraPermission::statusOf(Qt::PermissionStatus::Granted) == 2);
}

TEST_CASE("the camera permission constants are what the screens' own numbers say",
          "[qml][camera-permission]") {
    CHECK(CameraPermission::kUndetermined == 0);
    CHECK(CameraPermission::kDenied == 1);
    CHECK(CameraPermission::kGranted == 2);
}
