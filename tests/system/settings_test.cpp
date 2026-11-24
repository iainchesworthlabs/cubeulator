// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// Settings: opening and closing the screen, and what the controls on it do.

#include "app.hpp"

#include "app_runtime.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QStyleHints>

namespace {

using cube::systest::App;
using cube::systest::Layout;

void open_settings(App& app, const QString& from_control = "launch.settings") {
    app.click(from_control);
    REQUIRE(app.wait_for([&]() { return app.screen() == "settings"; }));
    app.settle();
}

[[nodiscard]] bool theme_is_dark(App& app) {
    return app.theme().property("effectiveDark").toBool();
}

// How much of what is behind an item shows through it and every item above it: the product
// of their opacities, its own included.
[[nodiscard]] double effective_opacity(const QQuickItem* item) {
    double seen = 1.0;
    for (; item != nullptr; item = item->parentItem()) {
        seen *= item->opacity();
    }
    return seen;
}

// What an AppButton is drawn in, read from its items (AppButton.qml): the plate, the label and the
// ring that marks the chosen one of a set when it is switched off.
struct Drawn {
    QColor fill;
    QColor edge;
    QColor label;
    bool ring_shown = false;
    double face_opacity = 1.0;
    double label_opacity = 1.0;
};

[[nodiscard]] Drawn drawn_of(const QQuickItem* button) {
    Drawn drawn;
    const QQuickItem* face = nullptr;
    for (const QQuickItem* child : button->childItems()) {
        if (!child->inherits("QQuickRectangle")) {
            continue;
        }
        if (child->objectName() == "appButton.chosenRing") {
            drawn.ring_shown = child->isVisible();
        } else if (face == nullptr) {
            face = child;
        }
    }
    REQUIRE(face != nullptr);
    drawn.fill = face->property("color").value<QColor>();
    drawn.edge =
        qvariant_cast<QObject*>(face->property("border"))->property("color").value<QColor>();
    drawn.face_opacity = effective_opacity(face);

    // The label is the Text that says what the button says.
    const QString said = button->property("text").toString();
    const auto find_label = [&said](const auto& self, const QQuickItem* from) -> const QQuickItem* {
        for (const QQuickItem* child : from->childItems()) {
            if (child->inherits("QQuickText") && child->property("text").toString() == said) {
                return child;
            }
            if (const QQuickItem* deeper = self(self, child)) {
                return deeper;
            }
        }
        return nullptr;
    };
    const QQuickItem* label = find_label(find_label, button);
    REQUIRE(label != nullptr);
    drawn.label = label->property("color").value<QColor>();
    drawn.label_opacity = effective_opacity(label);
    return drawn;
}

[[nodiscard]] QColor theme_colour(App& app, const char* name) {
    return app.theme().property(name).value<QColor>();
}

// A button that is switched off is an opaque `surface` plate with an `outline` edge and its label
// in `inkDisabled`, none of it faded, and the one chosen of a set has a ring inside its edge.
// Switched on, it has none of that.
void check_switched_off_style(App& app, const QQuickItem* button, bool chosen) {
    const Drawn drawn = drawn_of(button);
    CHECK(drawn.fill == theme_colour(app, "surface"));
    CHECK(drawn.fill.alpha() == 255);
    CHECK(drawn.edge == theme_colour(app, "outline"));
    CHECK(drawn.label == theme_colour(app, "inkDisabled"));
    CHECK(drawn.face_opacity == Catch::Approx(1.0));
    CHECK(drawn.label_opacity == Catch::Approx(1.0));
    CHECK(drawn.ring_shown == chosen);
}

void check_switched_on_style(App& app, const QQuickItem* button) {
    const Drawn drawn = drawn_of(button);
    CHECK(drawn.label != theme_colour(app, "inkDisabled"));
    CHECK(drawn.fill != theme_colour(app, "surface"));
    CHECK_FALSE(drawn.ring_shown);
    CHECK(drawn.face_opacity == Catch::Approx(1.0));
    CHECK(drawn.label_opacity == Catch::Approx(1.0));
}

}  // namespace

TEST_CASE("settings open from launch and close back to it", "[system][settings]") {
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        app.resize(layout == Layout::wide ? cube::systest::kDesktop : cube::systest::kPhone);

        open_settings(app);
        CHECK(app.find_text(qtTrId("common.settings")) != nullptr);
        CHECK(app.find("settings.back") != nullptr);
        CHECK(app.find("settings.theme.0") != nullptr);

        app.click("settings.back");
        REQUIRE(app.wait_for([&]() { return app.screen() == "launch"; }));
        CHECK(app.find("launch.startScan") != nullptr);
    }
}

TEST_CASE("the language button opens the settings too", "[system][settings]") {
    // Only English exists, so "English" on the launch screen is a shortcut to
    // the same screen. (When languages arrive it becomes a picker.)
    App& app = App::fresh();
    open_settings(app, "launch.language");
    CHECK(app.screen() == "settings");
    app.click("settings.back");
    CHECK(app.wait_for([&]() { return app.screen() == "launch"; }));
}

TEST_CASE("the theme switches live, and the choice is kept", "[system][settings][theme]") {
    App& app = App::fresh();
    open_settings(app);

    const QColor light_paper("#F3F2F2");
    const QColor dark_paper("#1A1918");
    CHECK_FALSE(theme_is_dark(app));
    CHECK(app.window().color() == light_paper);

    // Dark.
    app.click("settings.theme.2");
    REQUIRE(app.wait_for([&]() { return theme_is_dark(app); }));
    CHECK(app.window().color() == dark_paper);
    CHECK(app.settings_state().property("themeMode").toString() == "dark");
    // Still on the same screen, which now draws dark.
    CHECK(app.screen() == "settings");
    app.settle();
    const QImage dark = app.grab();
    CHECK(dark.pixelColor(2, 2).lightness() < 80);

    // The choice reached the store the next launch reads. The QML Settings
    // type writes a moment after a change (500 ms), not at once.
    const auto stored = []() {
        QSettings store;
        store.sync();
        return store.value("appearance/themeMode").toString();
    };
    CHECK(app.wait_for([&]() { return stored() == "dark"; }, 5000));

    // Light.
    app.click("settings.theme.1");
    REQUIRE(app.wait_for([&]() { return !theme_is_dark(app); }));
    CHECK(app.window().color() == light_paper);
    app.settle();
    CHECK(app.grab().pixelColor(2, 2).lightness() > 180);
    CHECK(app.wait_for([&]() { return stored() == "light"; }, 5000));

    // System follows the operating system's own setting.
    app.click("settings.theme.0");
    REQUIRE(app.wait_for(
        [&]() { return app.settings_state().property("themeMode").toString() == "system"; }));
    const bool os_dark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    CHECK(theme_is_dark(app) == os_dark);
}

TEST_CASE("a second launch comes up with the theme the first one left",
          "[.restart][system][settings][theme]") {
    // Run by its own ctest entry with CUBE_SYSTEM_TEST_THEME=dark in the
    // environment: main() has put that into the store before the app existed,
    // which is what a relaunch looks like to the app. The app is booted once
    // per process, so this case cannot make a second launch of its own.
    //
    // A run that picked it up through a tag it shares with the other cases
    // ([system], [settings], [theme]) has no seed, and nothing to say about a
    // relaunch: it is skipped, with the reason, not failed. A run of only this
    // case that skips it still fails (Catch2 exits 4 when nothing ran), so the
    // system.relaunch entry cannot pass without the check having been made.
    const QString seeded = qEnvironmentVariable("CUBE_SYSTEM_TEST_THEME");
    if (seeded.isEmpty()) {
        SKIP(
            "needs the store seeded before the app starts: run ctest -R relaunch, "
            "which sets CUBE_SYSTEM_TEST_THEME=dark");
    }
    REQUIRE(seeded == "dark");

    // Not fresh(): that resets to the baseline. This is the baseline, as
    // start-up read it.
    App& app = App::launched();
    REQUIRE(app.wait_for([&]() { return theme_is_dark(app); }));
    CHECK(app.settings_state().property("themeMode").toString() == "dark");
    // Nothing was said about motion, so it is the same as the device: the app's own
    // default, which every other entry puts out of reach by starting from "full"
    // (test_platform.hpp), and which follows whatever this machine's device asks.
    CHECK(app.settings_state().property("motion").toString() == "system");
    auto* device = app.runtime().engine().singletonInstance<QObject*>("Cubeulator", "SystemTheme");
    REQUIRE(device != nullptr);
    CHECK(app.theme().property("reducedMotion").toBool() ==
          device->property("reducedMotion").toBool());
    CHECK(app.window().color() == QColor("#1A1918"));
    app.settle();
    CHECK(app.grab().pixelColor(2, 2).lightness() < 80);
}

TEST_CASE("the diagnostics switches work from Settings, and every screen says while it writes",
          "[system][settings][diagnostics]") {
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        app.resize(layout == Layout::wide ? cube::systest::kDesktop : cube::systest::kPhone);
        auto& diagnostics = app.runtime().controllers().diagnostics;

        // Off at start, and the app shows nothing of it.
        REQUIRE_FALSE(diagnostics.enabled());
        CHECK(app.find("diagnostics.marker") == nullptr);

        open_settings(app);
        CHECK(app.find("settings.diagnostics") != nullptr);
        CHECK(app.find("settings.diagnosticsSnapshot") != nullptr);

        // Switched on while the app runs: the strip says so, and where.
        app.click("settings.diagnostics");
        REQUIRE(app.wait_for([&]() { return diagnostics.enabled(); }));
        app.settle();
        REQUIRE(app.find("diagnostics.marker") != nullptr);
        CHECK(app.find_text(qtTrId("diagnostics.marker").arg(diagnostics.path())) != nullptr);
        app.shot(QString("diagnostics-on-%1").arg(cube::systest::to_string(layout)));

        // A snapshot is in the file as soon as it is asked for.
        app.click("settings.diagnosticsSnapshot");
        QFile file(diagnostics.path());
        REQUIRE(file.open(QIODevice::ReadOnly));
        CHECK(file.readAll().contains("\"kind\":\"snapshot\""));

        // Frames are a switch of their own, and the strip says when they are.
        app.click("settings.diagnosticsFrames");
        REQUIRE(app.wait_for([&]() { return diagnostics.framesEnabled(); }));
        app.settle();
        CHECK(app.find_text(qtTrId("diagnostics.markerFrames").arg(diagnostics.directory())) !=
              nullptr);
        app.click("settings.diagnosticsFrames");
        REQUIRE(app.wait_for([&]() { return !diagnostics.framesEnabled(); }));

        // And off again, with no restart: the strip goes.
        app.click("settings.diagnostics");
        REQUIRE(app.wait_for([&]() { return !diagnostics.enabled(); }));
        app.settle();
        CHECK(app.find("diagnostics.marker") == nullptr);
    }
}

TEST_CASE("a control that is switched off is drawn in the switched-off style, and not faded",
          "[system][settings]") {
    // AppButton draws itself as a plain grey plate when it is disabled, and the speed chips are
    // disabled while the cube is not turning by itself. The row used to fade them as well, which
    // left a label nobody could read (the two opacities multiplied); a button is now not faded at
    // all, and nothing around one fades it either. Since AppButton's own enabled is the item's,
    // a chip under a disabled row reads as disabled and draws itself so.
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        app.resize(layout == Layout::wide ? cube::systest::kDesktop : cube::systest::kPhone);
        open_settings(app);

        // Clear history is disabled on its own account (nothing is kept) and shows it: a plain
        // button, none of it chosen.
        QQuickItem* alone = app.find("settings.clearHistory");
        REQUIRE(alone != nullptr);
        REQUIRE_FALSE(alone->isEnabled());
        check_switched_off_style(app, alone, false);
        CHECK(effective_opacity(alone) == Catch::Approx(1.0));

        // Nothing on the screen sits in a container that is faded: a button does not need it.
        const auto buttons_are_not_faded = [&]() {
            for (QQuickItem* control : app.visible_controls()) {
                if (!QByteArray(control->metaObject()->className()).startsWith("AppButton")) {
                    continue;
                }
                INFO("the container of " << app.control_key(control).toStdString()
                                         << " has opacity "
                                         << effective_opacity(control->parentItem()));
                CHECK(effective_opacity(control->parentItem()) == Catch::Approx(1.0));
            }
        };

        // The speed chips follow Auto-rotate, which App::fresh leaves off. Each is the switched-off
        // plate, and the one chosen is told from the others by its ring, not by a colour.
        buttons_are_not_faded();
        REQUIRE_FALSE(app.settings_state().property("cubeAutoRotate").toBool());
        const int chosen = app.settings_state().property("rotateSpeedIndex").toInt();
        for (int speed = 0; speed < 3; ++speed) {
            QQuickItem* chip = app.find(QStringLiteral("settings.rotateSpeed.%1").arg(speed));
            REQUIRE(chip != nullptr);
            INFO("speed chip " << speed << ", the chosen one is " << chosen);
            CHECK_FALSE(chip->isEnabled());
            CHECK(effective_opacity(chip) == Catch::Approx(1.0));
            check_switched_off_style(app, chip, speed == chosen);
        }

        // On, they are whole: the chosen one red, the others bare, no ring on any.
        app.click("settings.cubeAutoRotate");
        REQUIRE(app.wait_for(
            [&]() { return app.settings_state().property("cubeAutoRotate").toBool(); }));
        app.pump(100);
        for (int speed = 0; speed < 3; ++speed) {
            QQuickItem* chip = app.find(QStringLiteral("settings.rotateSpeed.%1").arg(speed));
            REQUIRE(chip != nullptr);
            INFO("speed chip " << speed);
            CHECK(chip->isEnabled());
            CHECK(effective_opacity(chip) == Catch::Approx(1.0));
            check_switched_on_style(app, chip);
        }

        buttons_are_not_faded();
        app.settings_state().setProperty("cubeAutoRotate", false);
    }
}
