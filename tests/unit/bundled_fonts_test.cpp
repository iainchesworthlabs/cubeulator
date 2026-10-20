// The design's Latin faces are compiled into the app and registered at
// start-up (kit/bundled_fonts.hpp, src/app/qt/fonts/), so that Theme.qml's
// Archivo and IBM Plex Mono are what is drawn whether or not the machine has
// them installed - and so that Qt is never asked for a family it does not have,
// which costs about 350 ms of start-up (docs/performance.md).
#include <catch2/catch_test_macros.hpp>

#include "kit/bundled_fonts.hpp"
#include "qml_test_app.hpp"

#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
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
