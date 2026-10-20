// The thread that asks Qt for the interface's fonts while the screens are
// built (kit/font_warmup.hpp), and that what it asks for is what Theme.qml
// names. The saving itself is start-up time, measured in docs/performance.md;
// what a test can hold is that the list does not drift from the QML, that the
// thread ends, and that it changes nothing about the face the text is set in.
#include <catch2/catch_test_macros.hpp>

#include "kit/font_warmup.hpp"
#include "qml_test_app.hpp"

#include <QFont>
#include <QFontInfo>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

namespace {

[[nodiscard]] QObject& theme() {
    auto* singleton = cube::test::qml_engine().singletonInstance<QObject*>("Cubeulator", "Theme");
    REQUIRE(singleton != nullptr);
    return *singleton;
}

// The face Qt sets text in when asked for `family`, as Qt itself resolves it.
[[nodiscard]] QString resolved(const QString& family) {
    return QFontInfo(QFont(family, 14)).family();
}

}  // namespace

TEST_CASE("The warm-up asks for the families Theme.qml names", "[fonts]") {
    const QStringList warmed = cube::app::kit::themeFamilies();

    CHECK(warmed.contains(theme().property("fontMono").toString()));

    // Japanese and Chinese are set in a system face chosen by language; every
    // other language is set in the heading face, which is what is warmed.
    const QString language = cube::test::qml_engine().uiLanguage().left(2).toLower();
    if (language != QStringLiteral("ja") && language != QStringLiteral("zh")) {
        CHECK(warmed.contains(theme().property("fontHeading").toString()));
    }
}

TEST_CASE("The warm-up ends and leaves the resolved faces as they were", "[fonts]") {
    const QStringList families = cube::app::kit::themeFamilies();
    QStringList before;
    for (const QString& family : families) {
        before.append(resolved(family));
    }

    {
        cube::app::kit::FontWarmup warmup(families, true);
        warmup.wait();
        warmup.wait();  // twice is fine
    }

    QStringList after;
    for (const QString& family : families) {
        after.append(resolved(family));
    }
    CHECK(after == before);
}

TEST_CASE("A warm-up that is switched off starts nothing", "[fonts]") {
    // Destroying it must not wait for, or join, a thread that was never made.
    const cube::app::kit::FontWarmup off(cube::app::kit::themeFamilies(), false);
    SUCCEED();
}

TEST_CASE("Warming no families is allowed", "[fonts]") {
    cube::app::kit::warmFonts({});
    const cube::app::kit::FontWarmup nothing({}, true);
    SUCCEED();
}
