// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// Accessibility: what a screen reader is told and what the keyboard can reach
// (docs/design/accessibility.md).
//
// Three kinds of case, all through the window the way a person's tools would:
//
//   * the accessibility tree - QAccessible, which is what every platform's
//     screen reader is built on - is walked, and every control on every screen
//     has to be in it with a role, a name and, where it is pressed, a place in
//     the tab order;
//   * the Tab key walks each screen, and has to land on every control;
//   * the whole journey (scan, solve, finish, review, scan again) is made with
//     the keyboard alone.
//
// What these cannot say is how a screen reader reads it. That is a person with
// NVDA, Narrator or VoiceOver (docs/design/accessibility.md, "Not tried").

#include "app.hpp"
#include "diagnostics.hpp"
#include "fake_camera.hpp"
#include "scan_script.hpp"

#include "app_runtime.hpp"
#include "i18n/language_manager.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cube/solver/solver.hpp>

#include <QAccessible>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>

#include <functional>
#include <map>
#include <vector>

namespace {

using cube::systest::App;
using cube::systest::FakeCamera;
using cube::systest::Layout;

constexpr int kScanTimeoutMs = 60'000;
constexpr int kSolveTimeoutMs = 60'000;

[[nodiscard]] QSize size_for(Layout layout) {
    return layout == Layout::wide ? cube::systest::kDesktop : cube::systest::kPhone;
}

// ---- the accessibility tree --------------------------------------------------

struct Node {
    QAccessible::Role role = QAccessible::NoRole;
    QString name;
    QString description;
    QAccessible::State state;
};

using Tree = std::map<const QObject*, Node>;

void collect(QAccessibleInterface* iface, Tree& out) {
    if (iface == nullptr) {
        return;
    }
    if (const QObject* object = iface->object(); object != nullptr) {
        out.emplace(object, Node{iface->role(), iface->text(QAccessible::Name),
                                 iface->text(QAccessible::Description), iface->state()});
    }
    for (int i = 0; i < iface->childCount(); ++i) {
        collect(iface->child(i), out);
    }
}

// Everything a screen reader would find in the window: what it lists, which is
// not what is merely there (an item that says Accessible.ignored, and an item
// with no Accessible of its own, are not in it).
[[nodiscard]] Tree accessibility_tree(App& app) {
    Tree tree;
    collect(QAccessible::queryAccessibleInterface(&app.window()), tree);
    return tree;
}

// The item that stands for `control` to a reader: the control itself, or the
// nearest item above it that is in the tree (a switch's pointer area is part of
// the switch).
struct Stand {
    QQuickItem* item = nullptr;
    const Node* node = nullptr;
};
[[nodiscard]] Stand stand_for(const Tree& tree, QQuickItem* control) {
    for (QQuickItem* up = control; up != nullptr; up = up->parentItem()) {
        if (const auto found = tree.find(up); found != tree.end()) {
            return {up, &found->second};
        }
    }
    return {};
}

[[nodiscard]] bool is_operable(QAccessible::Role role) {
    switch (role) {
        case QAccessible::Button:
        case QAccessible::RadioButton:
        case QAccessible::CheckBox:
        case QAccessible::ListItem:
        case QAccessible::Link:
        case QAccessible::Graphic:
            return true;
        default:
            return false;
    }
}

// An id that was never translated is a name nobody can use: "history.title".
[[nodiscard]] bool looks_like_an_id(const QString& name) {
    static const QRegularExpression id(QStringLiteral("^[a-z][A-Za-z0-9]*(\\.[A-Za-z0-9]+)+$"));
    return id.match(name).hasMatch();
}

// The cubes of the launch and settings screens turn when they are dragged, which
// is decoration (docs/design/accessibility.md: "Dragging the home cube is
// decoration"): they are not listed to a reader, and have no name to give.
[[nodiscard]] bool is_decorative(const QString& key) {
    return key.endsWith(":'cube'");
}

// Controls that are not reached with Tab, and why. Everything else a person can
// press is.
[[nodiscard]] bool reached_otherwise(const QString& key) {
    if (is_decorative(key)) {
        return true;
    }
    // The cubes are dragged to look round them: a way to look, for a pointer;
    // what they show is in the words (the turn sentence, the stickers' names).
    if (key == "solve.cube" || key == "colours.cube") {
        return true;
    }
    // The rows of the solution are stepped to with the screen's arrow keys, and a
    // screen reader can press them.
    if (key == "solution.list.row") {
        return true;
    }
    return false;
}

void expect_every_control_is_named(App& app, const QString& where) {
    INFO("on " << where.toStdString() << " in the " << cube::systest::to_string(app.layout())
               << " layout");
    const Tree tree = accessibility_tree(app);

    // The screen itself is a pane with a name: entering it says what it is.
    QQuickItem* screen = app.screen_item();
    REQUIRE(screen != nullptr);
    const auto pane = tree.find(screen);
    REQUIRE(pane != tree.end());
    CHECK(pane->second.role == QAccessible::Pane);
    CHECK_FALSE(pane->second.name.trimmed().isEmpty());

    // And it has a heading the reader can jump to.
    int headings = 0;
    for (const auto& [object, node] : tree) {
        if (node.role == QAccessible::Heading && !node.state.invisible &&
            !node.name.trimmed().isEmpty()) {
            ++headings;
        }
    }
    CHECK(headings >= 1);

    int controls = 0;
    for (QQuickItem* control : app.visible_controls()) {
        const QString key = app.control_key(control);
        INFO("the control " << key.toStdString());
        const Stand stand = stand_for(tree, control);
        if (is_decorative(key)) {
            // Not an operable thing to a reader: a picture that happens to move.
            CHECK((stand.node == nullptr || !is_operable(stand.node->role)));
            continue;
        }
        REQUIRE(stand.node != nullptr);
        ++controls;

        CHECK(is_operable(stand.node->role));
        CHECK_FALSE(stand.node->name.trimmed().isEmpty());
        CHECK_FALSE(looks_like_an_id(stand.node->name));
        INFO("named " << stand.node->name.toStdString());

        // What is pressed with a button is a button to the keyboard.
        const bool is_a_button = stand.node->role == QAccessible::Button ||
                                 stand.node->role == QAccessible::RadioButton ||
                                 stand.node->role == QAccessible::CheckBox;
        if (is_a_button && stand.item->isEnabled()) {
            CHECK(stand.node->state.focusable);
        }
        // Switched off to the eye is switched off to the reader.
        CHECK(stand.node->state.disabled == !stand.item->isEnabled());
    }
    CHECK(controls > 0);
}

// Tab round the screen, from the screen itself, until it comes back to where it
// began: the names of what it stopped on, in order.
[[nodiscard]] QStringList tab_round(App& app, int limit = 80, bool from_screen = true) {
    QQuickItem* screen = app.screen_item();
    REQUIRE(screen != nullptr);
    if (from_screen) {
        screen->forceActiveFocus();
        app.pump(40);
    }
    QStringList visited;
    for (int i = 0; i < limit; ++i) {
        app.key(Qt::Key_Tab);
        const QString name = app.focus_name();
        if (name.isEmpty() || visited.contains(name)) {
            break;
        }
        visited.append(name);
    }
    if (from_screen) {
        screen->forceActiveFocus();
    }
    return visited;
}

void expect_tab_reaches_every_control(App& app, const QString& where) {
    INFO("on " << where.toStdString() << " in the " << cube::systest::to_string(app.layout())
               << " layout");
    QSet<QString> wanted;
    for (QQuickItem* control : app.visible_controls()) {
        const QVariant own = control->property("enabled");
        if (!control->isEnabled() || (own.isValid() && !own.toBool())) {
            continue;
        }
        const QString key = app.control_key(control);
        if (!reached_otherwise(key)) {
            wanted.insert(key);
        }
    }
    const QStringList visited = tab_round(app);
    INFO("Tab stopped on: " << visited.join(", ").toStdString());
    for (const QString& key : wanted) {
        INFO("the control " << key.toStdString());
        if (key.endsWith(".row")) {
            // The rows of a list are one control to the reach gate ("licences.list.row")
            // and a stop each to Tab ("licences.row1", "licences.row2", ...).
            const QString prefix = key.left(key.indexOf('.')) + ".row";
            bool any = false;
            for (const QString& name : visited) {
                any = any || name.startsWith(prefix);
            }
            CHECK(any);
        } else {
            CHECK(visited.contains(key));
        }
    }
    // Tab goes on to something, and does not end in a trap.
    CHECK_FALSE(visited.isEmpty());
}

// ---- getting to the screens ----------------------------------------------------

void wait_for_screen(App& app, const char* name) {
    INFO("waiting for the " << name << " screen");
    REQUIRE(app.wait_for([&]() { return app.screen() == name; }));
    app.pump(80);
}

// A finished solve in the history, as the app keeps one (history_test.cpp).
void keep_a_finished_solve(App& app, unsigned seed) {
    auto& solve = app.runtime().solve();
    solve.solveCube(cube::systest::scramble(seed));
    REQUIRE(app.wait_for([&]() { return !solve.isSolving(); }, 180'000));
    app.app_state().setProperty("elapsedSeconds", 90);
    while (!solve.atEnd()) {
        solve.nextMove();
    }
    app.go("complete");
    app.go("launch");
}

// Every screen of the app, each once, by the way a person gets there; `visit`
// is called on each, with its name.
void each_screen(App& app, const std::function<void(const QString&)>& visit) {
    visit("launch");

    // Settings, and the About and Licences it leads to; Esc goes back, one at a
    // time (it is what the keyboard says "back" with).
    app.click("launch.settings");
    wait_for_screen(app, "settings");
    visit("settings");
    app.click("settings.about");
    wait_for_screen(app, "about");
    visit("about");
    app.click("about.licences");
    wait_for_screen(app, "licences");
    visit("licences");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "about");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "settings");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");

    // History: empty, then with a solve in it, and the question it asks.
    app.click("launch.solveHistory");
    wait_for_screen(app, "history");
    visit("history, empty");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");
    keep_a_finished_solve(app, 41);
    app.click("launch.solveHistory");
    wait_for_screen(app, "history");
    visit("history, with a solve");
    app.click("history.delete1");
    app.pump(120);
    visit("history, asking whether to delete");
    app.key(Qt::Key_Escape);
    app.pump(80);
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");

    // A cube of another size, and its colours by hand.
    app.click("launch.other");
    wait_for_screen(app, "entry");
    visit("entry");
    app.click("entry.colours");
    wait_for_screen(app, "colours");
    visit("colours");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "entry");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");

    // Where the platform asks for the camera first.
    app.go("permission");
    app.pump(80);
    visit("permission");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");

    // The scan, the solve, the solved screen and the review of it.
    app.click("launch.startScan");
    wait_for_screen(app, "scan");
    REQUIRE(app.wait_for([&]() { return app.runtime().directScan().running(); }));
    visit("scan");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");

    auto& solve = app.runtime().solve();
    solve.solveCube(cube::systest::scramble(43));
    REQUIRE(app.wait_for([&]() { return !solve.isSolving(); }, 180'000));
    app.go("solve");
    app.settle();
    visit("solve");
    app.go("complete");
    app.settle();
    visit("complete");
    app.click("complete.review");
    wait_for_screen(app, "solution");
    visit("solution");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "complete");
    app.go("launch");
}

}  // namespace

TEST_CASE("every control on every screen has a name and a role", "[system][accessibility]") {
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        static_cast<void>(cube::systest::take_qml_warnings());
        app.resize(size_for(layout));
        REQUIRE(app.layout() == layout);

        each_screen(app, [&](const QString& where) {
            // Entering a screen leaves focus on the screen, which is what a reader
            // starts reading from (a dialog, which has the focus of its own, is the
            // one thing that is not a screen).
            if (!where.contains("asking")) {
                QQuickItem* focused = app.focus_item();
                INFO("focus on " << where.toStdString());
                CHECK(focused != nullptr);
                const bool on_the_screen =
                    focused != nullptr && app.screen_item() != nullptr &&
                    (focused == app.screen_item() || app.screen_item()->isAncestorOf(focused));
                CHECK(on_the_screen);
            }
            expect_every_control_is_named(app, where);
        });

        const auto warnings = cube::systest::take_qml_warnings();
        for (const auto& warning : warnings) {
            UNSCOPED_INFO(warning.where.toStdString() << ": " << warning.message.toStdString());
        }
        CHECK(warnings.empty());
    }
}

TEST_CASE("Tab reaches every control on every screen", "[system][accessibility][keyboard]") {
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        static_cast<void>(cube::systest::take_qml_warnings());
        app.resize(size_for(layout));

        each_screen(app, [&](const QString& where) {
            // A dialog has its own focus, which stays inside it: that is its test.
            if (where.contains("asking")) {
                const QStringList round = tab_round(app, 10, false);
                INFO("Tab stopped on: " << round.join(", ").toStdString());
                CHECK(round.size() == 2);
                for (const QString& name : round) {
                    CHECK(name.startsWith("history.deleteDialog."));
                }
                return;
            }
            expect_tab_reaches_every_control(app, where);
        });
    }
}

TEST_CASE("Esc goes back, on every screen that has a way back",
          "[system][accessibility][keyboard]") {
    App& app = App::fresh();
    // The pairs are from the screen and to where Esc takes it; the buttons that
    // say the same ("Back", "Cancel", "Not now") are the same press.
    app.click("launch.settings");
    wait_for_screen(app, "settings");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");
    // And Android's back key, which Qt delivers as Key_Back.
    app.click("launch.settings");
    wait_for_screen(app, "settings");
    app.key(Qt::Key_Back);
    wait_for_screen(app, "launch");

    app.go("permission");
    app.pump(80);
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");

    // The scan is cancelled, and lets go of the camera.
    app.click("launch.startScan");
    wait_for_screen(app, "scan");
    REQUIRE(app.wait_for([&]() { return app.runtime().directScan().running(); }));
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");
    REQUIRE(app.wait_for([&]() { return !FakeCamera::instance().capturing(); }));
}

TEST_CASE("Settings is reached and left by keyboard, and shows where focus is",
          "[system][accessibility][keyboard]") {
    // What used to be asserted as it was (known_today_test.cpp: no back, and
    // nothing takes keyboard focus on the settings screen).
    App& app = App::fresh();
    app.click("launch.settings");
    wait_for_screen(app, "settings");
    app.screen_item()->forceActiveFocus();

    app.key(Qt::Key_Tab);
    QQuickItem* focused = app.focus_item();
    REQUIRE(focused != nullptr);
    CHECK(QByteArray(focused->metaObject()->className()).startsWith("AppButton"));
    // Its ring is on: the control that has focus says so.
    bool ring = false;
    for (QQuickItem* child : focused->childItems()) {
        if (QByteArray(child->metaObject()->className()).startsWith("FocusRing")) {
            ring = child->isVisible();
        }
    }
    CHECK(ring);

    // A press of Space, and of Enter, on a chip chooses it.
    REQUIRE(app.wait_for([&]() { return app.find("settings.theme") != nullptr; }));
    for (int i = 0; i < 40 && app.focus_name() != "settings.theme.2"; ++i) {
        app.key(Qt::Key_Tab);
    }
    REQUIRE(app.focus_name() == "settings.theme.2");
    app.press_focused(Qt::Key_Space);
    CHECK(app.settings_state().property("themeMode").toString() == "dark");
    // Shift+Tab to the one before it, and Enter.
    app.key(Qt::Key_Backtab, Qt::ShiftModifier);
    REQUIRE(app.focus_name() == "settings.theme.1");
    app.press_focused(Qt::Key_Return);
    CHECK(app.settings_state().property("themeMode").toString() == "light");
}

TEST_CASE("the whole journey is made by keyboard alone",
          "[system][accessibility][keyboard][journey]") {
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        static_cast<void>(cube::systest::take_qml_warnings());
        app.resize(size_for(layout));
        REQUIRE(app.layout() == layout);

        const cube::solver::Cube cube = cube::systest::scramble(7);
        auto& scan = app.runtime().directScan();
        auto& solve = app.runtime().solve();

        // Home: the first thing Tab reaches is Scan a cube, then the others in the
        // order they are on the screen, and Enter scans.
        app.screen_item()->forceActiveFocus();
        app.key(Qt::Key_Tab);
        CHECK(app.focus_name() == "launch.startScan");
        app.key(Qt::Key_Tab);
        CHECK(app.focus_name() == "launch.other");
        app.key(Qt::Key_Tab);
        CHECK(app.focus_name() == "launch.solveHistory");
        app.key(Qt::Key_Tab);
        CHECK(app.focus_name() == "launch.language");
        app.key(Qt::Key_Tab);
        CHECK(app.focus_name() == "launch.settings");
        // And back, with Shift+Tab.
        app.key(Qt::Key_Backtab, Qt::ShiftModifier);
        CHECK(app.focus_name() == "launch.language");
        for (int i = 0; i < 3; ++i) {
            app.key(Qt::Key_Backtab, Qt::ShiftModifier);
        }
        CHECK(app.focus_name() == "launch.startScan");
        app.press_focused(Qt::Key_Return);
        wait_for_screen(app, "scan");
        REQUIRE(app.wait_for([&]() { return scan.running(); }));

        // The scan has the keyboard's way out and its own buttons in order. The
        // person is holding the cube, not the keyboard, so what they do is turn
        // the cube; the camera does the rest.
        QQuickItem* screen = app.screen_item();
        CHECK(app.focus_item() == screen);
        cube::systest::show_side(cube::test::outside_view(cube, cube::solver::kAllFaces[0]));
        REQUIRE(app.wait_for([&]() { return scan.sidesRead() == 1; }, kScanTimeoutMs));
        cube::systest::show_empty_room();
        for (std::size_t i = 1; i < cube::solver::kAllFaces.size(); ++i) {
            cube::systest::show_side(cube::test::outside_view(cube, cube::solver::kAllFaces[i]));
            cube::systest::show_empty_room();
        }
        REQUIRE(app.wait_for([&]() { return scan.complete(); }, kScanTimeoutMs));
        REQUIRE(app.wait_for([&]() { return app.screen() == "solve"; }, kSolveTimeoutMs));
        REQUIRE_FALSE(solve.isSolving());
        REQUIRE(solve.totalMoves() > 0);
        app.settle();

        // Solve: keys that act on the screen, whatever has focus.
        CHECK(app.focus_item() == app.screen_item());
        const auto stepped = [&](int index) {
            INFO("waiting for move " << index);
            REQUIRE(app.wait_for([&]() { return solve.moveIndex() == index; }, 15'000));
        };
        app.key(Qt::Key_Right);
        stepped(1);
        app.key(Qt::Key_Space);
        stepped(2);
        app.key(Qt::Key_Return);
        stepped(3);
        app.key(Qt::Key_Left);
        stepped(2);

        // Tab to the buttons: Enter and Space press the one that has focus - and
        // only it, not the screen's own step as well.
        app.key(Qt::Key_Tab);
        QStringList seen;
        for (int i = 0; i < 30 && app.focus_name() != "solve.next"; ++i) {
            seen.append(app.focus_name());
            app.key(Qt::Key_Tab);
        }
        INFO("Tab passed: " << seen.join(", ").toStdString());
        REQUIRE(app.focus_name() == "solve.next");
        app.press_focused(Qt::Key_Return);
        stepped(3);
        app.press_focused(Qt::Key_Space);
        stepped(4);
        app.key(Qt::Key_Backtab, Qt::ShiftModifier);
        CHECK(app.focus_name() == "solve.previous");
        app.press_focused(Qt::Key_Return);
        stepped(3);

        // The rest of the moves by the controller (a key each is ninety turns
        // played out), and Finish by keyboard.
        const int total = solve.totalMoves();
        while (solve.moveIndex() < total - 1) {
            solve.nextMove();
        }
        app.pump(100);
        QQuickItem* next = app.find("solve.next");
        REQUIRE(next != nullptr);
        app.key(Qt::Key_Right);
        stepped(total);
        CHECK(solve.atEnd());
        CHECK(app.screen() == "solve");
        next = app.find("solve.next");
        REQUIRE(next != nullptr);
        for (int i = 0; i < 30 && app.focus_name() != "solve.next"; ++i) {
            app.key(Qt::Key_Tab);
        }
        REQUIRE(app.focus_name() == "solve.next");
        app.press_focused(Qt::Key_Return);
        wait_for_screen(app, "complete");

        // Complete: Tab goes to Review the solution, then Scan another cube.
        CHECK(app.focus_item() == app.screen_item());
        app.key(Qt::Key_Tab);
        CHECK(app.focus_name() == "complete.review");
        app.key(Qt::Key_Tab);
        CHECK(app.focus_name() == "complete.scanAnother");
        app.key(Qt::Key_Backtab, Qt::ShiftModifier);
        REQUIRE(app.focus_name() == "complete.review");
        app.press_focused(Qt::Key_Return);
        wait_for_screen(app, "solution");

        // The solution: the arrow keys step, Space plays and pauses, Home and End
        // go to the ends, Esc goes back.
        QQuickItem* solution = app.screen_item();
        REQUIRE(solution != nullptr);
        const auto position = [&]() { return solution->property("position").toInt(); };
        CHECK(position() == 0);
        app.key(Qt::Key_Right);
        REQUIRE(app.wait_for([&]() { return position() == 1; }, 15'000));
        app.key(Qt::Key_Left);
        REQUIRE(app.wait_for([&]() { return position() == 0; }, 15'000));
        app.key(Qt::Key_End);
        REQUIRE(app.wait_for([&]() { return position() == total; }, 15'000));
        app.key(Qt::Key_Home);
        REQUIRE(app.wait_for([&]() { return position() == 0; }, 15'000));
        app.key(Qt::Key_Space);
        CHECK(solution->property("playing").toBool());
        app.key(Qt::Key_Space);
        CHECK_FALSE(solution->property("playing").toBool());
        app.key(Qt::Key_Escape);
        wait_for_screen(app, "complete");

        // And round again: Tab twice, Enter on Scan another cube.
        app.key(Qt::Key_Tab);
        app.key(Qt::Key_Tab);
        REQUIRE(app.focus_name() == "complete.scanAnother");
        app.press_focused(Qt::Key_Return);
        wait_for_screen(app, "scan");
        REQUIRE(app.wait_for([&]() { return scan.running(); }));
        CHECK(scan.sidesRead() == 0);
        // Esc is Cancel.
        app.key(Qt::Key_Escape);
        wait_for_screen(app, "launch");

        const auto warnings = cube::systest::take_qml_warnings();
        for (const auto& warning : warnings) {
            UNSCOPED_INFO(warning.where.toStdString() << ": " << warning.message.toStdString());
        }
        CHECK(warnings.empty());
    }
}

TEST_CASE("the stickers of a side are painted by keyboard", "[system][accessibility][keyboard]") {
    App& app = App::fresh();
    static_cast<void>(cube::systest::take_qml_warnings());
    app.click("launch.other");
    wait_for_screen(app, "entry");
    // The size is the app's to remember between visits (AppState.entrySize), and an
    // earlier case may have chosen another.
    app.click("entry.size.3");
    app.click("entry.colours");
    wait_for_screen(app, "colours");
    QQuickItem* screen = app.screen_item();
    REQUIRE(screen != nullptr);

    const auto colour_of = [&](int index) {
        QQuickItem* sticker = app.find(QStringLiteral("colours.sticker.%1").arg(index));
        REQUIRE(sticker != nullptr);
        return sticker->property("colourOf").toString();
    };

    // One Tab stop for the side, and a cursor on its first sticker.
    screen->forceActiveFocus();
    for (int i = 0; i < 60 && app.focus_name() != "colours.grid"; ++i) {
        app.key(Qt::Key_Tab);
    }
    REQUIRE(app.focus_name() == "colours.grid");
    REQUIRE(screen->property("side").toString() == "F");

    // 2 chooses the second colour of the palette (yellow), Space paints the sticker the
    // cursor is on.
    app.key(Qt::Key_2);
    CHECK(screen->property("brush").toInt() == 1);
    CHECK(colour_of(0).isEmpty());
    app.press_focused(Qt::Key_Space);
    CHECK(colour_of(0) == "Yellow");

    // The arrow keys move the cursor over the stickers (not out of the side).
    QQuickItem* cursor_grid = app.find("colours.grid");
    REQUIRE(cursor_grid != nullptr);
    const auto cursor = [&]() { return cursor_grid->property("cursor").toInt(); };
    CHECK(cursor() == 0);
    app.key(Qt::Key_Left);
    app.key(Qt::Key_Up);
    CHECK(cursor() == 0);
    app.key(Qt::Key_Right);
    CHECK(cursor() == 1);
    app.key(Qt::Key_Down);
    CHECK(cursor() == 4);
    app.key(Qt::Key_3);
    CHECK(screen->property("brush").toInt() == 2);
    app.press_focused(Qt::Key_Return);
    CHECK(colour_of(4) == "Green");
    CHECK(colour_of(0) == "Yellow");

    // What is under the cursor is what the grid is named, for a reader.
    const Tree tree = accessibility_tree(app);
    QQuickItem* grid = app.find("colours.grid");
    REQUIRE(grid != nullptr);
    const auto node = tree.find(grid);
    REQUIRE(node != tree.end());
    CHECK(node->second.role == QAccessible::Button);
    CHECK(node->second.state.focusable);
    CHECK(node->second.name.contains(qtTrId("colour.green")));

    // Backspace takes the colour away; Page Down is the next side.
    app.key(Qt::Key_Backspace);
    CHECK(colour_of(4).isEmpty());
    app.key(Qt::Key_PageDown);
    CHECK(screen->property("side").toString() == "R");
    app.key(Qt::Key_PageUp);
    CHECK(screen->property("side").toString() == "F");

    // The palette is reached by Tab as well, and Space chooses a colour.
    for (int i = 0; i < 60 && app.focus_name() != "colours.colour.3"; ++i) {
        app.key(Qt::Key_Tab);
    }
    REQUIRE(app.focus_name() == "colours.colour.3");
    app.press_focused(Qt::Key_Space);
    CHECK(screen->property("brush").toInt() == 3);

    // Leaving with colours entered asks first; Esc is the safe answer, and a second
    // Esc then leaves.
    app.key(Qt::Key_Escape);
    app.pump(120);
    CHECK(app.find("colours.discard") != nullptr);
    app.key(Qt::Key_Escape);
    app.pump(120);
    CHECK(app.find("colours.discard") == nullptr);
    CHECK(app.screen() == "colours");
    app.go("launch");

    const auto warnings = cube::systest::take_qml_warnings();
    for (const auto& warning : warnings) {
        UNSCOPED_INFO(warning.where.toStdString() << ": " << warning.message.toStdString());
    }
    CHECK(warnings.empty());
}

// ---- what is said without being asked -------------------------------------------

namespace {

struct Said {
    QString message;
    QAccessible::AnnouncementPoliteness politeness = QAccessible::AnnouncementPoliteness::Polite;
    const QObject* from = nullptr;
};

std::vector<Said> g_said;
QAccessible::UpdateHandler g_handler_before = nullptr;

void remember_announcements(QAccessibleEvent* event) {
    if (event->type() == QAccessible::Announcement) {
        const auto* announcement = static_cast<const QAccessibleAnnouncementEvent*>(event);
        g_said.push_back({announcement->message(), announcement->politeness(), event->object()});
    }
}

// Listens for what the app tells a screen reader: Accessible.announce raises a
// QAccessibleAnnouncementEvent, which each platform's backend turns into its own
// notification (UI Automation's, NSAccessibility's, UIAccessibility's). Here a
// handler takes them instead, so what was said, and how urgently, can be read.
class Listening {
public:
    Listening() {
        g_said.clear();
        g_handler_before = QAccessible::installUpdateHandler(&remember_announcements);
    }
    ~Listening() { QAccessible::installUpdateHandler(g_handler_before); }
    Listening(const Listening&) = delete;
    Listening& operator=(const Listening&) = delete;

    [[nodiscard]] static bool said(const QString& words) {
        for (const Said& one : g_said) {
            if (one.message.contains(words)) {
                return true;
            }
        }
        return false;
    }
    [[nodiscard]] static QString everything() {
        QStringList all;
        for (const Said& one : g_said) {
            all.append(one.message);
        }
        return all.join(" | ");
    }
};

}  // namespace

TEST_CASE("the scan says what it says, when it says it", "[system][accessibility]") {
    App& app = App::fresh();
    static_cast<void>(cube::systest::take_qml_warnings());
    const Listening listening;
    auto& scan = app.runtime().directScan();
    const cube::solver::Cube cube = cube::systest::scramble(7);

    app.click("launch.startScan");
    wait_for_screen(app, "scan");
    REQUIRE(app.wait_for([&]() { return scan.running(); }));
    QQuickItem* screen = app.screen_item();
    REQUIRE(screen != nullptr);

    // The headline and the guidance, which are what a person looking sees change,
    // are said; the headline with it when it is new.
    cube::systest::show_side(cube::test::outside_view(cube, cube::solver::kAllFaces[0]));
    REQUIRE(app.wait_for([&]() { return scan.sidesRead() == 1; }, kScanTimeoutMs));
    cube::systest::show_empty_room();
    const QString headline = screen->property("headline").toString();
    REQUIRE_FALSE(headline.isEmpty());
    INFO("said: " << Listening::everything().toStdString());
    CHECK(app.wait_for([&]() { return Listening::said(headline); }, 5000));
    // Politely: nothing here stops the person. From the screen, which a reader
    // lists and which has a name.
    for (const Said& one : g_said) {
        CHECK(one.politeness == QAccessible::AnnouncementPoliteness::Polite);
        CHECK(one.from == screen);
    }

    // The picture and the six chips are named, and say what they are.
    const Tree tree = accessibility_tree(app);
    int pictures = 0;
    int named_chips = 0;
    QStringList chips;
    for (const auto& [object, node] : tree) {
        pictures += node.name == qtTrId("scan.a11y.preview") && node.role == QAccessible::Graphic;
    }
    for (int side = 1; side <= 6; ++side) {
        for (const auto& [object, node] : tree) {
            const bool chip =
                node.role == QAccessible::Graphic && !node.state.invisible &&
                (node.name == qtTrId("scan.a11y.chipEmpty").arg(side) ||
                 node.name.startsWith(qtTrId("scan.a11y.chipRead").arg(side).left(8)));
            if (chip) {
                ++named_chips;
                chips.append(node.name);
                break;
            }
        }
    }
    CHECK(pictures >= 1);
    INFO("chips named: " << chips.join(" | ").toStdString());
    CHECK(named_chips == 6);

    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");
}

TEST_CASE("a camera that will not start is said at once, and interrupts",
          "[system][accessibility]") {
    App& app = App::fresh();
    const Listening listening;
    auto& scan = app.runtime().directScan();

    FakeCamera::instance().fail_next_start(cube::CubeError::camera_start_failed);
    app.click("launch.startScan");
    wait_for_screen(app, "scan");
    REQUIRE(app.wait_for([&]() { return !scan.cameraError().isEmpty() && !scan.starting(); }));
    INFO("said: " << Listening::everything().toStdString());
    REQUIRE(app.wait_for([&]() { return Listening::said(scan.cameraError()); }, 5000));
    bool interrupted = false;
    for (const Said& one : g_said) {
        if (one.message.contains(scan.cameraError())) {
            interrupted = one.politeness == QAccessible::AnnouncementPoliteness::Assertive;
        }
    }
    CHECK(interrupted);
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");
}

TEST_CASE("each turn of the solve is said as it comes", "[system][accessibility]") {
    App& app = App::fresh(App::Start::elsewhere);
    auto& solve = app.runtime().solve();
    solve.solveCube(cube::systest::scramble(17));
    REQUIRE(app.wait_for([&]() { return !solve.isSolving(); }, kSolveTimeoutMs));
    const Listening listening;
    app.go("solve");
    app.settle();
    QQuickItem* screen = app.screen_item();
    REQUIRE(screen != nullptr);

    // The turn to make, with where it is in the solve.
    const QString first = screen->property("moveSpeech").toString();
    const QString first_place = screen->property("progressSpeech").toString();
    REQUIRE_FALSE(first.isEmpty());
    REQUIRE_FALSE(first_place.isEmpty());
    INFO("said: " << Listening::everything().toStdString());
    CHECK(app.wait_for([&]() { return Listening::said(first) && Listening::said(first_place); },
                       5000));

    // And the next, when the person steps: by key, with focus on the screen.
    g_said.clear();
    app.key(Qt::Key_Right);
    REQUIRE(app.wait_for([&]() { return solve.moveIndex() == 1; }, 15'000));
    const QString second_place = screen->property("progressSpeech").toString();
    REQUIRE_FALSE(second_place.isEmpty());
    CHECK(second_place != first_place);
    INFO("said: " << Listening::everything().toStdString());
    CHECK(app.wait_for([&]() { return Listening::said(second_place); }, 5000));

    // The cube itself is a picture, and says what it is a picture of.
    const Tree tree = accessibility_tree(app);
    QQuickItem* cube = app.find("solve.cube");
    REQUIRE(cube != nullptr);
    const auto node = tree.find(cube);
    REQUIRE(node != tree.end());
    CHECK(node->second.role == QAccessible::Graphic);
    CHECK(node->second.name.contains(screen->property("moveSpeech").toString()));
    app.go("launch");
}

TEST_CASE("the solution says each turn as it moves", "[system][accessibility]") {
    App& app = App::fresh(App::Start::elsewhere);
    auto& solve = app.runtime().solve();
    solve.solveCube(cube::systest::scramble(19));
    REQUIRE(app.wait_for([&]() { return !solve.isSolving(); }, kSolveTimeoutMs));
    app.go("complete");
    app.settle();
    app.click("complete.review");
    wait_for_screen(app, "solution");
    const Listening listening;
    QQuickItem* screen = app.screen_item();
    REQUIRE(screen != nullptr);

    app.key(Qt::Key_Right);
    REQUIRE(app.wait_for([&]() { return screen->property("position").toInt() == 1; }, 15'000));
    const QString caption = screen->property("caption").toString();
    REQUIRE_FALSE(caption.isEmpty());
    INFO("said: " << Listening::everything().toStdString());
    CHECK(app.wait_for([&]() { return Listening::said(caption); }, 5000));
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "complete");
    app.go("launch");
}

TEST_CASE("what the solve says of a turn is said in the app's language",
          "[system][accessibility]") {
    // The announcement is built from the screen's own words (the turn in words, which way
    // clockwise is, how far through the solve), so it follows the language: a reader in
    // Hebrew is not given English.
    App& app = App::fresh(App::Start::elsewhere);
    auto* language = app.runtime().engine().singletonInstance<cube::i18n::LanguageManager*>(
        "Cubeulator", "LanguageManager");
    REQUIRE(language != nullptr);
    auto& solve = app.runtime().solve();
    solve.solveCube(cube::systest::scramble(17));
    REQUIRE(app.wait_for([&]() { return !solve.isSolving(); }, kSolveTimeoutMs));
    // A quarter turn, which has a direction to say; a half turn has none.
    while (solve.currentMoveTurns() == 2 && !solve.atEnd()) {
        solve.nextMove();
    }
    REQUIRE_FALSE(solve.atEnd());

    for (const char* code : {"de", "fr", "es", "ja", "zh_CN", "he", "ar", "yi"}) {
        INFO("in " << code);
        language->preview(QString::fromLatin1(code));
        app.pump(80);
        // Listening first: the screen says its turn a moment after it appears.
        const Listening listening;
        app.go("solve");
        app.settle();
        QQuickItem* screen = app.screen_item();
        REQUIRE(screen != nullptr);

        // What the sentence is made of is not the English.
        const QString sentence = qtTrId("solve.a11y.move");
        CHECK(sentence != "%1, %2 as you look at that side.");
        CHECK(qtTrId("dir.cw") != "clockwise");

        const QString expected =
            sentence.arg(screen->property("moveWords").toString())
                .arg(qtTrId(solve.currentMoveTurns() == 1 ? "dir.cw" : "dir.ccw"));
        const QString speech = screen->property("moveSpeech").toString();
        CHECK(speech == expected);
        CHECK_FALSE(speech.contains("as you look at that side"));
        CHECK_FALSE(speech.contains("clockwise"));
        REQUIRE_FALSE(speech.isEmpty());

        // And it is that that is announced, with the place in the solve in the same words.
        const QString place = screen->property("progressSpeech").toString();
        INFO("said: " << Listening::everything().toStdString());
        CHECK(app.wait_for([&]() { return Listening::said(speech) && Listening::said(place); },
                           5000));
        for (const Said& one : g_said) {
            CHECK_FALSE(one.message.contains("Turn "));
            CHECK_FALSE(one.message.contains("clockwise"));
        }
        app.go("launch");
    }
    app.reset_language();
}

TEST_CASE("what is wrong with a typed scramble is said, and interrupts",
          "[system][accessibility]") {
    App& app = App::fresh();
    app.click("launch.other");
    wait_for_screen(app, "entry");
    const Listening listening;

    // Nothing typed.
    app.click("entry.solve");
    app.pump(120);
    QQuickItem* screen = app.screen_item();
    REQUIRE(screen != nullptr);
    const QString words = screen->property("problemWords").toString();
    REQUIRE_FALSE(words.isEmpty());
    INFO("said: " << Listening::everything().toStdString());
    REQUIRE(app.wait_for([&]() { return Listening::said(words); }, 5000));
    CHECK(g_said.back().politeness == QAccessible::AnnouncementPoliteness::Assertive);

    // The box is named, and what it wants is its description.
    const Tree tree = accessibility_tree(app);
    QQuickItem* box = app.find("entry.scramble");
    REQUIRE(box != nullptr);
    const Stand stand = stand_for(tree, box);
    REQUIRE(stand.node != nullptr);
    CHECK_FALSE(stand.node->name.isEmpty());
    CHECK_FALSE(stand.node->description.isEmpty());
    app.key(Qt::Key_Escape);
}

TEST_CASE("a solve deleted from the history is said, twice if it is deleted twice",
          "[system][accessibility]") {
    App& app = App::fresh();
    keep_a_finished_solve(app, 41);
    keep_a_finished_solve(app, 42);
    app.click("launch.solveHistory");
    wait_for_screen(app, "history");
    const Listening listening;

    for (int deleted = 1; deleted <= 2; ++deleted) {
        // By keyboard: Tab to the row's delete button, Enter, and Enter on the
        // dialog's second button (the dialog starts on the safe one).
        app.screen_item()->forceActiveFocus();
        for (int i = 0; i < 12 && app.focus_name() != "history.delete1"; ++i) {
            app.key(Qt::Key_Tab);
        }
        REQUIRE(app.focus_name() == "history.delete1");
        app.press_focused(Qt::Key_Return);
        REQUIRE(app.wait_for([&]() { return app.find("history.deleteDialog") != nullptr; }));
        app.key(Qt::Key_Tab);
        REQUIRE(app.focus_name() == "history.deleteDialog.keep");
        app.key(Qt::Key_Tab);
        REQUIRE(app.focus_name() == "history.deleteDialog.confirm");
        app.press_focused(Qt::Key_Space);
        REQUIRE(app.wait_for([&]() { return app.runtime().history().count() == 2 - deleted; }));
        INFO("said: " << Listening::everything().toStdString());
        REQUIRE(app.wait_for(
            [&]() {
                int times = 0;
                for (const Said& one : g_said) {
                    times += one.message == qtTrId("history.a11y.deleted") ? 1 : 0;
                }
                return times == deleted;
            },
            5000));
        if (deleted == 1) {
            app.pump(100);
        }
    }
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");
}

// ---- less motion --------------------------------------------------------------

namespace {

[[nodiscard]] QQuickItem* find_class(QQuickItem* root, const char* prefix) {
    if (QByteArray(root->metaObject()->className()).startsWith(prefix)) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = find_class(child, prefix)) {
            return found;
        }
    }
    return nullptr;
}

// A QML object of a kind (a Timer, an animation) that belongs to `owner`, which
// has this property equal to this.
[[nodiscard]] QObject* owned(QObject* owner, const char* class_prefix, const char* property,
                             const QVariant& equals) {
    for (QObject* child : owner->findChildren<QObject*>()) {
        if (QByteArray(child->metaObject()->className()).startsWith(class_prefix) &&
            child->property(property) == equals) {
            return child;
        }
    }
    return nullptr;
}

void set_motion(App& app, const char* mode) {
    app.settings_state().setProperty("motion", mode);
    app.pump(120);
}

[[nodiscard]] bool reduced(App& app) {
    return app.theme().property("reducedMotion").toBool();
}

}  // namespace

TEST_CASE("Settings has a Motion row, which is kept and which Theme follows",
          "[system][accessibility][motion]") {
    const Layout layout = GENERATE(Layout::wide, Layout::narrow);
    DYNAMIC_SECTION("in the " << cube::systest::to_string(layout) << " layout") {
        App& app = App::fresh();
        static_cast<void>(cube::systest::take_qml_warnings());
        app.resize(size_for(layout));
        // Every case begins from "full" (App::fresh), whatever this machine asks for: a
        // hosted Windows runner is a virtual machine with its animations off, and Qt
        // reports that as a request for less motion.
        CHECK(app.settings_state().property("motion").toString() == "full");
        CHECK_FALSE(reduced(app));
        // "Same as this device" follows whatever the device says, which is not the same on
        // every machine: the case reads what the app was told (SystemTheme, which is Qt's
        // report of the platform's setting) and compares, and never assumes it.
        auto* device =
            app.runtime().engine().singletonInstance<QObject*>("Cubeulator", "SystemTheme");
        REQUIRE(device != nullptr);
        const bool device_asks = device->property("reducedMotion").toBool();
        // The row offers the same as the device first, then reduced, then full.
        const QVariant modes = app.settings_state().property("motionModes");
        REQUIRE(modes.isValid());
        CHECK(modes.toStringList() == QStringList({"system", "reduced", "full"}));
        set_motion(app, "system");
        CHECK(reduced(app) == device_asks);

        app.click("launch.settings");
        wait_for_screen(app, "settings");
        app.click("settings.motion.1");
        CHECK(app.settings_state().property("motion").toString() == "reduced");
        CHECK(reduced(app));
        app.click("settings.motion.2");
        CHECK(app.settings_state().property("motion").toString() == "full");
        CHECK_FALSE(reduced(app));
        app.click("settings.motion.0");
        CHECK(app.settings_state().property("motion").toString() == "system");
        CHECK(reduced(app) == device_asks);

        // The choice is the app's, for next time; the writing waits half a second.
        app.click("settings.motion.1");
        app.pump(900);
        QSettings store;
        store.sync();
        CHECK(store.value("appearance/motion").toString() == "reduced");

        const auto warnings = cube::systest::take_qml_warnings();
        for (const auto& warning : warnings) {
            UNSCOPED_INFO(warning.where.toStdString() << ": " << warning.message.toStdString());
        }
        CHECK(warnings.empty());
    }
}

TEST_CASE("less motion stills the cube's idle spin", "[system][accessibility][motion]") {
    App& app = App::fresh();
    app.settings_state().setProperty("cubeAutoRotate", true);
    set_motion(app, "full");
    REQUIRE(app.wait_for([&]() { return find_class(app.screen_item(), "Cube3DView") != nullptr; }));
    QQuickItem* cube = find_class(app.screen_item(), "Cube3DView");
    REQUIRE(cube != nullptr);
    QObject* spin = owned(cube, "QQmlTimer", "interval", 16);
    REQUIRE(spin != nullptr);

    CHECK(spin->property("running").toBool());
    set_motion(app, "reduced");
    CHECK_FALSE(spin->property("running").toBool());
    set_motion(app, "full");
    CHECK(spin->property("running").toBool());
    app.settings_state().setProperty("cubeAutoRotate", false);
}

TEST_CASE("less motion draws the layer to turn leaning, still, and turns snap",
          "[system][accessibility][motion]") {
    App& app = App::fresh(App::Start::elsewhere);
    auto& solve = app.runtime().solve();
    solve.solveCube(cube::systest::scramble(17));
    REQUIRE(app.wait_for([&]() { return !solve.isSolving(); }, kSolveTimeoutMs));
    set_motion(app, "full");
    app.go("solve");
    app.settle();
    QQuickItem* cube = app.find("solve.cube");
    REQUIRE(cube != nullptr);

    // As it always was: the layer nudges, over and over.
    CHECK(app.wait_for([&]() { return cube->property("nudging").toBool(); }, 5000));

    // Asked for less while it is on screen: it stops, and is drawn leaning.
    set_motion(app, "reduced");
    CHECK(app.wait_for([&]() { return !cube->property("nudging").toBool(); }, 5000));
    CHECK(
        app.wait_for([&]() { return std::abs(cube->property("turnAngle").toReal()) > 1.0; }, 5000));
    const qreal lean = cube->property("turnAngle").toReal();
    app.pump(400);
    CHECK(cube->property("turnAngle").toReal() == lean);  // still

    // A turn snaps: the press lands at once, not a third of a second later.
    app.click("solve.next");
    REQUIRE(app.wait_for([&]() { return solve.moveIndex() == 1; }, 2000));

    // And back to as it was.
    set_motion(app, "full");
    CHECK(app.wait_for([&]() { return cube->property("nudging").toBool(); }, 5000));
    app.go("launch");
}

TEST_CASE("less motion stops what loops on the finished screen",
          "[system][accessibility][motion]") {
    App& app = App::fresh(App::Start::elsewhere);
    app.runtime().solve().solveCube(cube::systest::scramble(17));
    REQUIRE(app.wait_for([&]() { return !app.runtime().solve().isSolving(); }, kSolveTimeoutMs));

    const auto looping = [&]() {
        int running = 0;
        for (QObject* child : app.screen_item()->findChildren<QObject*>()) {
            if (QByteArray(child->metaObject()->className())
                    .startsWith("QQuickSequentialAnimation") &&
                child->property("running").toBool()) {
                ++running;
            }
        }
        return running;
    };

    set_motion(app, "full");
    app.go("complete");
    app.settle();
    CHECK(looping() > 0);
    set_motion(app, "reduced");
    CHECK(looping() == 0);
    set_motion(app, "full");
    CHECK(looping() > 0);
    app.go("launch");
}

// ---- for a person to look at -----------------------------------------------------

// Pictures of the focus ring: a primary, a secondary and a ghost button, a chip, a switch
// and a row, light and dark, and the scan screen's buttons over the camera. Written to
// <screenshot dir> (CUBE_SYSTEM_TEST_OUT); a person judges them. Hidden, so a plain run
// does not take them: `cube_system_tests "[.focus-shot]"`.
TEST_CASE("pictures of the focus ring", "[.focus-shot]") {
    App& app = App::fresh();
    const cube::solver::Cube cube = cube::systest::scramble(7);
    const auto stop_on = [&](const QString& name) {
        for (int i = 0; i < 80 && app.focus_name() != name; ++i) {
            app.key(Qt::Key_Tab);
        }
        REQUIRE(app.focus_name() == name);
        app.settle();
    };
    for (const char* theme : {"light", "dark"}) {
        app.set_theme(theme);
        app.go("launch");
        app.settle();
        app.screen_item()->forceActiveFocus();
        for (const char* name : {"launch.startScan", "launch.other", "launch.language"}) {
            stop_on(name);
            app.shot(QStringLiteral("focus-%1-%2").arg(name, theme));
        }

        app.go("settings");
        app.settle();
        app.screen_item()->forceActiveFocus();
        for (const char* name : {"settings.theme.1", "settings.keepHistory", "settings.about"}) {
            stop_on(name);
            app.shot(QStringLiteral("focus-%1-%2").arg(name, theme));
        }

        app.go("complete");
        app.settle();
        app.screen_item()->forceActiveFocus();
        stop_on("complete.review");
        app.shot(QStringLiteral("focus-complete.review-%1").arg(theme));
    }

    app.set_theme("light");
    app.go("launch");
    app.click("launch.startScan");
    wait_for_screen(app, "scan");
    REQUIRE(app.wait_for([&]() { return app.runtime().directScan().running(); }));
    cube::systest::show_side(cube::test::outside_view(cube, cube::solver::kAllFaces[0]));
    app.pump(800);
    app.screen_item()->forceActiveFocus();
    stop_on("scan.size.auto");
    app.shot("focus-scan.size.auto");
    stop_on("scan.buttons");
    app.shot("focus-scan.buttons");
    app.key(Qt::Key_Escape);
    wait_for_screen(app, "launch");
}
