// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "SystemTheme.hpp"

#include <QAccessibilityHints>
#include <QGuiApplication>
#include <QStyleHints>

SystemTheme::SystemTheme(QObject* parent) : QObject(parent) {
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { emit darkChanged(); });
    connect(qApp->styleHints()->accessibility(), &QAccessibilityHints::motionPreferenceChanged,
            this, [this](Qt::MotionPreference) { emit reducedMotionChanged(); });
}

bool SystemTheme::isDark() const {
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

bool SystemTheme::isReducedMotion() const {
    return qApp->styleHints()->accessibility()->motionPreference() ==
           Qt::MotionPreference::ReducedMotion;
}
