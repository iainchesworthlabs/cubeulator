// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// What the About and Licences screens show (controllers/about_info.hpp): the
// generated third-party notices, read for the screens, and nothing made up.
#include <catch2/catch_test_macros.hpp>

#include "controllers/about_info.hpp"

#include <cube/version.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QVariantMap>

namespace {

// A notices file as the build writes one, small enough to read here.
[[nodiscard]] QByteArray notices_json() {
    const QJsonObject project{{"name", "Cubeulator"},
                              {"license", "GPL-3.0-or-later"},
                              {"copyright", "Copyright (C) 2026 Somebody"},
                              {"source", "https://example.org/source"}};
    const QJsonArray components{
        QJsonObject{{"id", "zlib"},
                    {"kind", "vcpkg"},
                    {"name", "zlib"},
                    {"version", "1.3.1"},
                    {"license", "Zlib"},
                    {"distributedUnder", "Zlib"},
                    {"copyright", "Copyright (C) Jean-loup Gailly and Mark Adler"},
                    {"text", "zlib's own words."},
                    {"licenseRefs", QJsonArray{"Zlib"}}},
        QJsonObject{{"id", "font-archivo"},
                    {"kind", "font"},
                    {"name", "Archivo (typeface)"},
                    {"version", "commit 2111276"},
                    {"license", "OFL-1.1"},
                    {"copyright", "Copyright 2020 The Archivo Project Authors"},
                    {"text", ""},
                    {"licenseRefs", QJsonArray{}}},
        QJsonObject{{"id", "qt/qtbase"},
                    {"kind", "qt"},
                    {"name", "Qt: Qt Base"},
                    {"version", "6.12.0"},
                    {"license", "LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-3.0-only"},
                    {"distributedUnder", "GPL-3.0-only"},
                    {"copyright", "Copyright (C) The Qt Company Ltd."},
                    {"text", ""},
                    {"licenseRefs", QJsonArray{"GPL-3.0-only"}}},
        QJsonObject{{"id", "unnamed"}, {"name", ""}},
    };
    const QJsonObject texts{{"Zlib", "The zlib licence in full."},
                            {"GPL-3.0-only", "GNU GENERAL PUBLIC LICENSE"},
                            {"GPL-3.0-or-later", "GNU GENERAL PUBLIC LICENSE, or later."}};
    return QJsonDocument(QJsonObject{{"schema", 1},
                                     {"project", project},
                                     {"components", components},
                                     {"licenseTexts", texts}})
        .toJson();
}

[[nodiscard]] QString write(const QTemporaryDir& dir, const QByteArray& bytes) {
    const QString path = dir.filePath(QStringLiteral("notices.json"));
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    file.write(bytes);
    return path;
}

}  // namespace

TEST_CASE("The About information names the app and carries the build's version", "[about]") {
    const cube::app::AboutInfo about(QStringLiteral("missing.json"));
    CHECK(about.name() == QStringLiteral("Cubeulator"));
    CHECK(about.version().toStdString() == std::string(cube::version_string));
    // The publisher is the line in cmake/AppIdentity.cmake, passed in by CMake.
    CHECK_FALSE(about.publisher().isEmpty());
}

TEST_CASE("The components are read from the notices, by name, with their texts", "[about]") {
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const cube::app::AboutInfo about(write(dir, notices_json()));

    REQUIRE(about.noticesAvailable());
    CHECK(about.sourceUrl() == QStringLiteral("https://example.org/source"));
    CHECK(about.licenceId() == QStringLiteral("GPL-3.0-or-later"));

    const QVariantMap own = about.own();
    CHECK(own.value("name").toString() == QStringLiteral("Cubeulator"));
    CHECK(own.value("licence").toString() == QStringLiteral("GPL-3.0-or-later"));
    CHECK(own.value("text").toString().contains(QStringLiteral("Copyright (C) 2026 Somebody")));
    CHECK(own.value("text").toString().contains(QStringLiteral("or later")));

    // Alphabetical, without the entry that has no name.
    const QVariantList components = about.components();
    REQUIRE(components.size() == 3);
    CHECK(components[0].toMap().value("name").toString() == QStringLiteral("Archivo (typeface)"));
    CHECK(components[1].toMap().value("name").toString() == QStringLiteral("Qt: Qt Base"));
    CHECK(components[2].toMap().value("name").toString() == QStringLiteral("zlib"));

    // A font: the licence it is under, its copyright line (it has no text of its
    // own, so the line is the notice).
    const QVariantMap font = components[0].toMap();
    CHECK(font.value("kind").toString() == QStringLiteral("font"));
    CHECK(font.value("licence").toString() == QStringLiteral("OFL-1.1"));
    CHECK(font.value("version").toString() == QStringLiteral("commit 2111276"));
    CHECK(font.value("text").toString().contains(QStringLiteral("The Archivo Project Authors")));

    // What the app is distributed under, not the expression offered; and the
    // licence text the component refers to follows its own.
    const QVariantMap qt = components[1].toMap();
    CHECK(qt.value("licence").toString() == QStringLiteral("GPL-3.0-only"));
    CHECK(qt.value("text").toString().contains(QStringLiteral("GNU GENERAL PUBLIC LICENSE")));

    const QVariantMap zlib = components[2].toMap();
    const QString zlib_text = zlib.value("text").toString();
    CHECK(zlib_text.startsWith(QStringLiteral("zlib's own words.")));
    CHECK(zlib_text.contains(QStringLiteral("The zlib licence in full.")));
}

TEST_CASE("Notices that cannot be read are said to be unavailable, with the source's address",
          "[about]") {
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());

    SECTION("a file that is not there") {
        const cube::app::AboutInfo about(dir.filePath(QStringLiteral("nothing.json")));
        CHECK_FALSE(about.noticesAvailable());
        CHECK(about.components().isEmpty());
        // The address is still known: the screen says where they are published.
        CHECK(about.sourceUrl().startsWith(QStringLiteral("https://")));
    }
    SECTION("text that is not JSON") {
        const cube::app::AboutInfo about(write(dir, "not json"));
        CHECK_FALSE(about.noticesAvailable());
    }
    SECTION("the placeholder the build writes before it has generated the notices") {
        const cube::app::AboutInfo about(
            write(dir, R"({"schema":1,"components":[],"unresolved":["not generated yet"]})"));
        CHECK_FALSE(about.noticesAvailable());
    }
}

TEST_CASE("Changing where the notices are read from tells the screens", "[about]") {
    const QTemporaryDir dir;
    REQUIRE(dir.isValid());
    cube::app::AboutInfo about(dir.filePath(QStringLiteral("nothing.json")));
    REQUIRE_FALSE(about.noticesAvailable());

    int changed = 0;
    QObject::connect(&about, &cube::app::AboutInfo::noticesChanged, [&changed]() { ++changed; });
    about.setNoticesPath(write(dir, notices_json()));
    CHECK(changed == 1);
    CHECK(about.noticesAvailable());
    CHECK(about.components().size() == 3);
}
