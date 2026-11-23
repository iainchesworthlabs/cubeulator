// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qqmlintegration.h>
#include <QObject>

// Whether the OS is currently in dark mode, for Settings' "System (default)"
// appearance option. Deliberately not just a QML binding on
// Qt.styleHints.colorScheme: Qt's own docs note that property's change
// notifier isn't implemented for QML, so a binding on it won't update live
// when the user flips their OS theme while the app is running. This wraps
// the real QStyleHints::colorSchemeChanged C++ signal instead, so it does.
//
// The same goes for the operating system's request for less motion (Windows'
// "Show animations in Windows", macOS and iOS "Reduce motion", Android's
// "Remove animations", the browser's prefers-reduced-motion): Qt 6.12 reports
// it as QAccessibilityHints::motionPreference, and what each platform plugin
// reads is its own. A platform whose plugin reads nothing reports no
// preference, which is why Settings has a Motion row of its own as well.
class SystemTheme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool dark READ isDark NOTIFY darkChanged)
    Q_PROPERTY(bool reducedMotion READ isReducedMotion NOTIFY reducedMotionChanged)

public:
    explicit SystemTheme(QObject* parent = nullptr);

    [[nodiscard]] bool isDark() const;
    [[nodiscard]] bool isReducedMotion() const;

signals:
    void darkChanged();
    void reducedMotionChanged();
};
