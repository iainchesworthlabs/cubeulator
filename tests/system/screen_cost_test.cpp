// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// What it costs to arrive on each screen, and what a screen costs to leave on.
//
// For every screen a person can reach, the window is brought to it from another
// screen and three things are timed (docs/performance.md, "Screens"):
//
//   call     how long the GUI thread was held by the navigation itself - the
//            Loader creating the screen's items, which nothing can be drawn or
//            answered during;
//   first    the time to the first swapped frame after it, and `tenth` to the
//            tenth: the screen's own first-frame cost (shaders, the 3D view's
//            first sync) on top of `call`;
//   worst    the longest gap between two of those ten frames;
//
// and then, with nothing asking for a frame, how many frames the screen draws by
// itself in a second (`idle`): a static screen should draw none, a cube turning
// by itself draws every frame, and which is which is the point. `ceiling` is the
// opposite: the frames in a second when every swap asks for the next, which is
// one over what a frame of that screen costs on this renderer.
//
// Not part of the walk: it does nothing unless CUBE_SCREEN_COST is set.
//
//   CUBE_SCREEN_COST=1 cube_system_tests "[screen-cost]"
//
// CUBE_SCREEN_COST_REPS is how many times each is brought up (3; the first of
// a process pays for what the others find compiled). Like probe_cost_test.cpp
// it measures whatever renderer the platform picks.

#include "app.hpp"

#include "app_runtime.hpp"
#include "solve_controller.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QElapsedTimer>
#include <QMetaObject>
#include <QObject>
#include <QQuickWindow>
#include <QString>
#include <QVariant>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <vector>

using cube::systest::App;

namespace {

constexpr std::size_t kFrames = 10;

[[nodiscard]] double seconds(const QElapsedTimer& timer) {
    return static_cast<double>(timer.nsecsElapsed()) / 1.0e9;
}

struct Case {
    const char* name;
    // Moves the window to the screen from wherever it is; the work timed as `call`.
    std::function<void(App&)> arrive;
    // True once the screen is the one asked for, and whatever it started has
    // landed.
    std::function<bool(App&)> arrived;
    // Where to stand before arriving. The screen is left for the permission
    // screen between repetitions, which draws nothing.
    std::function<void(App&)> prepare = nullptr;
};

void measure(const Case& c, int repetitions, App::Scene3D scene) {
    for (int rep = 0; rep < repetitions; ++rep) {
        App& app = App::fresh(App::Start::elsewhere, scene);
        if (c.prepare) {
            c.prepare(app);
        }
        std::vector<double> swaps;
        QElapsedTimer timer;
        timer.start();
        const auto connection = QObject::connect(&app.window(), &QQuickWindow::frameSwapped,
                                                 [&]() { swaps.push_back(seconds(timer)); });

        c.arrive(app);
        const double call = seconds(timer);
        const bool done = app.wait_for(
            [&]() {
                if (swaps.size() < kFrames) {
                    app.window().update();
                }
                return c.arrived(app) && swaps.size() >= kFrames;
            },
            600000);
        double worst_gap = 0.0;
        for (std::size_t i = 1; i < swaps.size(); ++i) {
            worst_gap = std::max(worst_gap, swaps[i] - swaps[i - 1]);
        }
        const double first = swaps.empty() ? -1.0 : swaps.front();
        const double tenth = swaps.size() >= kFrames ? swaps[kFrames - 1] : -1.0;

        // Nothing asks for a frame now; whatever is drawn, the screen drew itself.
        // The fade in is 280 ms, so wait it out first.
        app.pump(600);
        QObject::disconnect(connection);
        std::size_t idle = 0;
        const auto idle_connection =
            QObject::connect(&app.window(), &QQuickWindow::frameSwapped, [&]() { ++idle; });
        app.pump(1000);
        QObject::disconnect(idle_connection);

        // And the most it can draw: every swap asks for the next frame, with
        // vsync off (the harness's), so the count is one over the cost of a
        // frame - what the screen costs on this renderer, CPU and GPU together.
        std::size_t ceiling = 0;
        const auto ceiling_connection =
            QObject::connect(&app.window(), &QQuickWindow::frameSwapped, [&]() {
                ++ceiling;
                app.window().update();
            });
        app.window().update();
        app.pump(1000);
        QObject::disconnect(ceiling_connection);

        std::printf(
            "screen-cost: api=%s screen=%s rep=%d call=%.0fms first=%.0fms tenth=%.0fms "
            "worst_gap=%.0fms idle=%zu/s ceiling=%zu/s done=%d\n",
            qPrintable(app.graphics_api()), c.name, rep, call * 1000.0, first * 1000.0,
            tenth * 1000.0, worst_gap * 1000.0, idle, ceiling, done ? 1 : 0);
        std::fflush(stdout);
        CHECK(done);
    }
}

void solve_scramble(App& app, int size) {
    QVariant result;
    REQUIRE(QMetaObject::invokeMethod(
        &app.app_state(), "solveScramble", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, size),
        Q_ARG(QVariant, QStringLiteral("R U R' U' 2R F2 3Rw' 2-3L D2 Bw L' 2U"))));
    REQUIRE(result.toMap().value("ok").toBool());
}

}  // namespace

TEST_CASE("what arriving on each screen costs", "[.][screen-cost]") {
    if (!qEnvironmentVariableIsSet("CUBE_SCREEN_COST")) {
        SUCCEED("CUBE_SCREEN_COST is not set: nothing measured");
        return;
    }
    const int reps = qEnvironmentVariableIsSet("CUBE_SCREEN_COST_REPS")
                         ? qEnvironmentVariableIntValue("CUBE_SCREEN_COST_REPS")
                         : 3;
    const auto on = [](const char* screen) {
        return [screen](App& a) { return a.screen() == QLatin1String(screen); };
    };
    const auto go = [](const char* screen) {
        // Not App::go, which also pumps events for 60 ms: that is not the navigation.
        return
            [screen](App& a) { a.app_state().setProperty("screen", QString::fromLatin1(screen)); };
    };
    const auto solved = [](App& a) {
        return a.screen() == QLatin1String("solve") && !a.runtime().solve().isSolving();
    };

    const std::vector<Case> cases{
        {"launch", go("launch"), on("launch")},
        {"entry", go("entry"), on("entry")},
        {"settings", go("settings"), on("settings")},
        {"history", go("history"), on("history")},
        {"scan", go("scan"), on("scan")},
        {"solve3", [](App& a) { solve_scramble(a, 3); }, solved},
        {"solve6", [](App& a) { solve_scramble(a, 6); }, solved},
        {"solution3",
         [](App& a) {
             QMetaObject::invokeMethod(&a.app_state(), "openSolution", Q_ARG(QVariant, QVariant()),
                                       Q_ARG(QVariant, QVariant()));
         },
         on("solution"),
         [](App& a) {
             solve_scramble(a, 3);
             REQUIRE(a.wait_for([&]() { return !a.runtime().solve().isSolving(); }, 180'000));
             a.go("permission");
         }},
    };
    for (const App::Scene3D scene : {App::Scene3D::full}) {
        for (const Case& c : cases) {
            measure(c, reps, scene);
        }
    }
}
