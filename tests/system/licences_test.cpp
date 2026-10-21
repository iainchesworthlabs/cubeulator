// About and the Licences screen under it, in the real window (docs/design/screens/
// about.md and licences.md): reached from Settings, the way to the notices the app
// owes to what it is built from, with every component that ships - the bundled
// fonts among them - readable in full.
//
// Replaces the placeholder About dialog of E4a, which named the app and its
// version and closed again; the version, and that it goes back where it came
// from, are still held here.

#include "app.hpp"

#include "app_runtime.hpp"
#include "controllers/about_info.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace {

using cube::systest::App;
using cube::systest::Layout;

[[nodiscard]] QSize size_for(Layout layout) {
    return layout == Layout::wide ? cube::systest::kDesktop : cube::systest::kPhone;
}

[[nodiscard]] QString version_line(App& app) {
    return qtTrId("about.version").arg(app.runtime().about().displayVersion());
}

[[nodiscard]] QString focused(App& app) {
    QQuickItem* item = app.window().activeFocusItem();
    return item == nullptr ? QString() : item->objectName();
}

// To About, from Settings, the way a person gets there on Windows and Linux.
void open_about(App& app) {
    app.click("launch.settings");
    REQUIRE(app.wait_for([&]() { return app.screen() == "settings"; }));
    app.settle();
    app.click("settings.about");
    REQUIRE(app.wait_for([&]() { return app.screen() == "about"; }));
    app.settle();
}

}  // namespace

TEST_CASE("About names the app, its version and its publisher, and goes back to Settings",
          "[system][about]") {
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        app.resize(size_for(layout));
        open_about(app);

        // The version is the build's, not a string the QML makes up.
        CHECK(app.find_text(QStringLiteral("Cubeulator")) != nullptr);
        QQuickItem* version = app.find("about.version");
        REQUIRE(version != nullptr);
        CHECK(version->property("text").toString() == version_line(app));
        QQuickItem* author = app.find("about.author");
        REQUIRE(author != nullptr);
        CHECK(author->property("text").toString().contains(app.runtime().about().publisher()));
        // The GPL obliges the app to say where its source is, on the screen.
        QQuickItem* source = app.find("about.source");
        REQUIRE(source != nullptr);
        CHECK(source->property("value").toString() == app.runtime().about().sourceUrl());

        app.click("about.back");
        REQUIRE(app.wait_for([&]() { return app.screen() == "settings"; }));
    }
}

TEST_CASE("Licences lists every component and every bundled font, and opens each to its notice",
          "[system][about][licences]") {
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        app.resize(size_for(layout));
        open_about(app);

        app.click("about.licences");
        REQUIRE(app.wait_for([&]() { return app.screen() == "licences"; }));
        app.settle();
        REQUIRE(app.find("licences.list") != nullptr);
        CHECK(app.find("licences.detail") == nullptr);  // the list, not a notice

        // Cubeulator's own row, and then a row for each component.
        QQuickItem* own = app.find("licences.own");
        REQUIRE(own != nullptr);
        CHECK(app.find("licences.row1") != nullptr);

        // Archivo and IBM Plex Mono are bundled with the app, so they are in the
        // list: found there by the name the build gave them, and opened.
        const QVariantList components = app.runtime().about().components();
        for (const QString& family : {QStringLiteral("Archivo"), QStringLiteral("IBM Plex Mono")}) {
            INFO("font: " << family.toStdString());
            int row = 0;
            for (int i = 0; i < components.size(); ++i) {
                const QVariantMap component = components[i].toMap();
                if (component.value("kind").toString() == QStringLiteral("font") &&
                    component.value("name").toString().startsWith(family)) {
                    row = i + 1;  // row 0 is Cubeulator's own
                }
            }
            REQUIRE(row > 0);

            // A row far down a scrolling list is made when it comes into view,
            // so the screen is asked to open it (a press on a row scrolls to it
            // first, below).
            REQUIRE(QMetaObject::invokeMethod(app.screen_item(), "open",
                                              Q_ARG(QVariant, QVariant(row))));
            app.settle();
            REQUIRE(app.screen_item()->property("reading").toBool());
            QQuickItem* item = app.find("licences.text");
            REQUIRE(item != nullptr);
            const QString text = item->property("text").toString();
            CHECK(text.contains(QStringLiteral("SIL OPEN FONT LICENSE Version 1.1")));
            CHECK(text.contains(components[row - 1].toMap().value("copyright").toString()));

            // Back: to the list, and the screen is still Licences.
            app.click("licences.back");
            REQUIRE(
                app.wait_for([&]() { return !app.screen_item()->property("reading").toBool(); }));
            CHECK(app.screen() == "licences");
        }

        // A row of the list pressed in the window opens its notice.
        app.click("licences.own");
        REQUIRE(app.wait_for([&]() { return app.find("licences.detail") != nullptr; }));
        QQuickItem* text = app.find("licences.text");
        REQUIRE(text != nullptr);
        CHECK(text->property("text").toString().contains(
            QStringLiteral("GNU GENERAL PUBLIC LICENSE")));

        // Back from the notice, then from the list, then from About.
        app.click("licences.back");
        REQUIRE(app.wait_for([&]() { return app.find("licences.detail") == nullptr; }));
        app.click("licences.back");
        REQUIRE(app.wait_for([&]() { return app.screen() == "about"; }));
        app.click("about.back");
        REQUIRE(app.wait_for([&]() { return app.screen() == "settings"; }));
    }
}

TEST_CASE("About and Licences are reached and left with the keyboard",
          "[system][about][licences]") {
    App& app = App::fresh();
    app.resize(cube::systest::kDesktop);
    open_about(app);

    // Tab reaches the Licences row; Return presses it.
    for (int i = 0; i < 6 && focused(app) != QStringLiteral("about.licences"); ++i) {
        app.key(Qt::Key_Tab);
        app.pump(30);
    }
    REQUIRE(focused(app) == QStringLiteral("about.licences"));
    app.key(Qt::Key_Return);
    REQUIRE(app.wait_for([&]() { return app.screen() == "licences"; }));
    app.settle();

    // Down moves from row to row (the first is Cubeulator's own); Return opens.
    for (int i = 0; i < 6 && focused(app) != QStringLiteral("licences.own"); ++i) {
        app.key(Qt::Key_Tab);
        app.pump(30);
    }
    REQUIRE(focused(app) == QStringLiteral("licences.own"));
    app.key(Qt::Key_Down);
    app.pump(30);
    CHECK(focused(app) == QStringLiteral("licences.row1"));
    app.key(Qt::Key_Up);
    app.pump(30);
    CHECK(focused(app) == QStringLiteral("licences.own"));
    app.key(Qt::Key_Return);
    REQUIRE(app.wait_for([&]() { return app.find("licences.detail") != nullptr; }));

    // The notice has the keyboard: the page keys scroll it, Escape leaves it,
    // and Escape again leaves the list for About, and again About for Settings.
    CHECK(focused(app) == QStringLiteral("licences.text"));
    app.key(Qt::Key_PageDown);
    app.pump(30);
    app.key(Qt::Key_Escape);
    REQUIRE(app.wait_for([&]() { return app.find("licences.detail") == nullptr; }));
    CHECK(focused(app) == QStringLiteral("licences.own"));  // back at the row it left
    app.key(Qt::Key_Escape);
    REQUIRE(app.wait_for([&]() { return app.screen() == "about"; }));
    app.key(Qt::Key_Escape);
    REQUIRE(app.wait_for([&]() { return app.screen() == "settings"; }));
}

TEST_CASE("Licences says so when its notices are missing, and where else they are",
          "[system][about][licences]") {
    App& app = App::fresh();
    app.resize(cube::systest::kPhone);
    app.runtime().about().setNoticesPath(QStringLiteral("no-such-notices.json"));
    app.go("licences");
    app.pump(80);
    CHECK(app.find("licences.unavailable") != nullptr);
    CHECK(app.find("licences.list") == nullptr);
    CHECK(
        app.find_text(qtTrId("licences.unavailable.body").arg(app.runtime().about().sourceUrl())) !=
        nullptr);
    app.runtime().about().setNoticesPath(cube::app::AboutInfo::defaultNoticesPath());
}
