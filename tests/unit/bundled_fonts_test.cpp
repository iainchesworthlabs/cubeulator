// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// The design's Latin faces are compiled into the app and registered at
// start-up (kit/bundled_fonts.hpp, src/app/qt/fonts/), so that Theme.qml's
// Archivo and IBM Plex Mono are what is drawn whether or not the machine has
// them installed - and so that Qt is never asked for a family it does not have,
// which costs about 350 ms of start-up (docs/performance.md).
#include <catch2/catch_test_macros.hpp>

#include "kit/bundled_fonts.hpp"
#include "qml_test_app.hpp"

#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

namespace {

// What Qt resolves a request for this family and weight to.
[[nodiscard]] QFontInfo resolve(const QString& family, int weight) {
    QFont font(family);
    font.setPixelSize(16);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return QFontInfo(font);
}

[[nodiscard]] QObject& theme() {
    auto* singleton = cube::test::qml_engine().singletonInstance<QObject*>("Cubeulator", "Theme");
    REQUIRE(singleton != nullptr);
    return *singleton;
}

}  // namespace

TEST_CASE("The bundle holds Archivo and IBM Plex Mono", "[fonts]") {
    const QStringList families = cube::app::kit::installBundledFonts();
    INFO("registered: " << families.join(", ").toStdString());
    CHECK(families.contains(QStringLiteral("Archivo")));
    CHECK(families.contains(QStringLiteral("IBM Plex Mono")));
    CHECK(QFontDatabase::hasFamily(QStringLiteral("Archivo")));
    CHECK(QFontDatabase::hasFamily(QStringLiteral("IBM Plex Mono")));
}

TEST_CASE("Installing the bundle twice adds nothing twice", "[fonts]") {
    const QStringList once = cube::app::kit::installBundledFonts();
    const QStringList twice = cube::app::kit::installBundledFonts();
    CHECK(once == twice);
}

TEST_CASE("Archivo's three weights each resolve to their own face", "[fonts]") {
    cube::app::kit::installBundledFonts();
    // Regular for running text, Bold for headlines and direction, ExtraBold
    // for display, titles and labels (src/app/qt/fonts/README.md).
    for (const int weight : {400, 700, 800}) {
        const QFontInfo info = resolve(QStringLiteral("Archivo"), weight);
        INFO("weight " << weight << " resolved to '" << info.family().toStdString() << "' "
                       << info.styleName().toStdString() << " at " << info.weight());
        CHECK(info.family().startsWith(QStringLiteral("Archivo")));
        if (weight == 800 && info.weight() == 700) {
            // CoreText (macOS) lists the ExtraBold file under the Bold style of
            // the family, so a request for 800 is answered with the 700 face; it
            // is the heaviest the family offers there, and the text is still set
            // in Archivo. Said, not failed: it is the platform's font engine.
            WARN("Archivo ExtraBold is not kept apart from Bold on this platform");
            continue;
        }
        CHECK(info.weight() == weight);
    }
}

TEST_CASE("The mono face is IBM Plex Mono Medium", "[fonts]") {
    cube::app::kit::installBundledFonts();
    const QFontInfo info = resolve(QStringLiteral("IBM Plex Mono"), 500);
    INFO("resolved to '" << info.family().toStdString() << "' " << info.styleName().toStdString()
                         << " at " << info.weight());
    CHECK(info.family().startsWith(QStringLiteral("IBM Plex Mono")));
    CHECK(info.weight() == 500);
    CHECK(info.fixedPitch());
}

TEST_CASE("The families Theme.qml names are the ones the bundle carries", "[fonts]") {
    const QStringList families = cube::app::kit::installBundledFonts();

    CHECK(families.contains(theme().property("fontMono").toString()));

    // Japanese and Chinese are set in a system face chosen by language; every
    // other language is set in the heading face.
    const QString language = cube::test::qml_engine().uiLanguage().left(2).toLower();
    if (language != QStringLiteral("ja") && language != QStringLiteral("zh")) {
        CHECK(families.contains(theme().property("fontHeading").toString()));
    }
}

// The notices the build generates are what cube_app embeds as
// :/licences/third-party-notices.json for the screens that list them, and what
// the desktop packages carry as THIRD-PARTY-NOTICES.txt. The OFL asks that a
// font's licence and copyright line travel with it, so a font that is bundled
// and not in them is a licence breach, and the build's own scan (the
// third_party_notices_complete test) is held to the same by this one, from the
// side of the fonts the app actually registers.
TEST_CASE("Every bundled font is in the notices, with its licence text", "[fonts][licences]") {
    QFile file(QStringLiteral(CUBE_NOTICES_JSON));
    REQUIRE(file.open(QIODevice::ReadOnly));
    const QJsonObject notices = QJsonDocument::fromJson(file.readAll()).object();
    const QJsonArray components = notices.value(QStringLiteral("components")).toArray();
    REQUIRE_FALSE(components.isEmpty());

    const QStringList families = cube::app::kit::installBundledFonts();
    REQUIRE_FALSE(families.isEmpty());

    for (const QString& family : families) {
        INFO("family: " << family.toStdString());
        QJsonObject found;
        for (const QJsonValue& value : components) {
            const QJsonObject component = value.toObject();
            // "Archivo (typeface)" is the notice for "Archivo", and for the
            // legacy names its weights register under ("Archivo ExtraBold").
            const QString name = component.value(QStringLiteral("name"))
                                     .toString()
                                     .section(QStringLiteral(" ("), 0, 0);
            if (component.value(QStringLiteral("kind")).toString() == QStringLiteral("font") &&
                family.startsWith(name)) {
                found = component;
            }
        }
        REQUIRE_FALSE(found.isEmpty());

        const QString copyright = found.value(QStringLiteral("copyright")).toString();
        const QString text = found.value(QStringLiteral("text")).toString();
        CHECK(found.value(QStringLiteral("license")).toString() == QStringLiteral("OFL-1.1"));
        CHECK_FALSE(copyright.isEmpty());
        // The notice is the copyright line and then the licence in full.
        CHECK(text.contains(copyright));
        CHECK(text.contains(QStringLiteral("SIL OPEN FONT LICENSE Version 1.1")));
        CHECK(text.contains(QStringLiteral("DISCLAIMER")));
    }

    // No font the generator could not attribute.
    for (const QJsonValue& value : notices.value(QStringLiteral("unresolved")).toArray()) {
        CHECK_FALSE(value.toString().contains(QStringLiteral("Font file")));
    }
}
