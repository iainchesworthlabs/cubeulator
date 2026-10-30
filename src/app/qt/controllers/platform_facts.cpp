// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "controllers/platform_facts.hpp"

#include "interfaces/platform_info.hpp"

#include <QString>
#include <QVariantMap>

namespace cube::app {

PlatformFacts::PlatformFacts(QObject* parent) : QObject(parent) {
    const auto info = cube::platform::platform_info::current();
    is_mobile_ = info.is_mobile;
    camera_ids_withheld_until_use_ = info.camera_ids_withheld_until_use;
    camera_switch_on_scan_screen_ = info.camera_switch_on_scan_screen;
    for (const auto& backend : info.graphics_backends) {
        graphics_backends_.append(QVariantMap{
            {QStringLiteral("key"), QString::fromStdString(backend.key)},
            {QStringLiteral("label"), QString::fromStdString(backend.label)},
        });
    }
    for (const auto& face : info.japanese_faces) {
        japanese_faces_.append(QString::fromStdString(face));
    }
    for (const auto& face : info.chinese_faces) {
        chinese_faces_.append(QString::fromStdString(face));
    }
}

bool PlatformFacts::isMobile() const noexcept {
    return is_mobile_;
}

bool PlatformFacts::cameraIdsWithheldUntilUse() const noexcept {
    return camera_ids_withheld_until_use_;
}

bool PlatformFacts::cameraSwitchOnScanScreen() const noexcept {
    return camera_switch_on_scan_screen_;
}

QVariantList PlatformFacts::graphicsBackends() const {
    return graphics_backends_;
}

QStringList PlatformFacts::japaneseFaces() const {
    return japanese_faces_;
}

QStringList PlatformFacts::chineseFaces() const {
    return chinese_faces_;
}

}  // namespace cube::app
