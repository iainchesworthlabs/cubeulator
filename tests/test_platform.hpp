// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "cube_test_platform_facts.hpp"

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <string_view>

namespace cube::test {

// Gives a Qt test binary its platform's default Qt platform plugin when
// QT_QPA_PLATFORM is not already set. Which plugin that is, if any, is the
// platform's to say (CUBEULATOR_PLATFORM_TEST_QPA_PLATFORM in
// src/app/platform/<os>/platform.cmake, carried here by
// cube_test_platform_facts.hpp), so the binaries do not ask which OS they
// are on.
inline void apply_default_qpa_platform() {
    constexpr std::string_view qpa = platform::default_qpa_platform;
    if (!qpa.empty() && !qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArray(qpa.data(), static_cast<qsizetype>(qpa.size())));
    }
}

// Starts every test that loads the app's own QML from the same Motion setting,
// whatever the machine says. The app's Settings > Appearance > Motion is "same
// as this device" until a person changes it, and the device may ask for less
// motion: a hosted Windows runner is a virtual machine with "Show animations in
// Windows" off, so Qt reports ReducedMotion there and the nudge and the sweep of
// a turn - which the cube, solve and solution tests wait for - never happen. A
// test about the layer moving is about the layer moving, so it starts from "full"
// (docs/testing.md, "Motion"); a test about less motion sets "reduced" or "system"
// itself. Call it once, after QSettings has been pointed at a store of the
// process's own and before the QML engine makes the settings singleton, which
// reads the store when it is made.
inline void pin_full_motion() {
    QSettings store;
    store.setValue(QStringLiteral("appearance/motion"), QStringLiteral("full"));
    store.sync();
}

// If a test that draws the real app cannot, because this platform's desktop
// session has not reached the process, why - and what to do about it, with
// `program` named in the advice. Empty when it can, which is always so where
// the platform names no session variable. A QT_QPA_PLATFORM chosen by hand
// picks a plugin that has its own idea of a display, so it is left alone.
[[nodiscard]] inline QString display_problem(const QString& program) {
    constexpr std::string_view variables = platform::display_variables;
    if (variables.empty() || qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        return {};
    }
    const QString names =
        QString::fromUtf8(variables.data(), static_cast<qsizetype>(variables.size()));
    for (const QString& name : names.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        if (qEnvironmentVariableIsSet(name.toUtf8().constData())) {
            return {};
        }
    }
    QString help = QString::fromUtf8(platform::display_help.data(),
                                     static_cast<qsizetype>(platform::display_help.size()));
    help.replace(QStringLiteral("PROGRAM"), program);
    return QStringLiteral("needs a display, and none of %1 is set: %2")
        .arg(names.split(QLatin1Char(' '), Qt::SkipEmptyParts).join(QStringLiteral(", ")), help);
}

}  // namespace cube::test
