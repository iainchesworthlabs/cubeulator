// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// What the app does today that the experience plan says it should not.
//
// Each case asserts the behaviour as it IS. They are written to start failing
// the day the thing is fixed - which is the prompt to turn the case into a
// proper journey step and delete it from here. Every one names the line of
// docs/app-experience-plan.md it comes from. None of them is fixed by this
// workstream: the experience lane owns them, each in its own phase.
//
// Five things that used to be here are gone: the solve lane's Finish step now
// reaches the complete screen, its Rescan is the way out of the solve screen,
// Review the solution on the complete screen opens the solution (all three
// journey steps, journey_test.cpp), the History button on Home opens the
// list of solves kept (history_test.cpp), and the settings screen has a back
// (Esc and the Back key) and takes keyboard focus on its buttons
// (accessibility_test.cpp, which is the journey step it became).
//
// Tagged [known-today] so `cube_system_tests "[known-today]"` lists them.

#include "app.hpp"

#include "app_runtime.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using cube::systest::App;

TEST_CASE("known today: buttons that do nothing", "[system][known-today]") {
    // EXPECTED TO CHANGE - plan, Journey: "'Solve history', 'Review
    // solution' and the 'English' button do nothing or the wrong thing".
    // 'Review solution' opens the solution now (journey_test.cpp) and 'Solve
    // history' opens the list (history_test.cpp); the English button is the
    // experience lane's, E1 decides what stays (decision 6).
    const cube::systest::Layout layout =
        GENERATE(cube::systest::Layout::wide, cube::systest::Layout::narrow);
    INFO("in the " << cube::systest::to_string(layout) << " layout");
    App& app = App::fresh();
    app.resize(layout == cube::systest::Layout::wide ? cube::systest::kDesktop
                                                     : cube::systest::kPhone);

    // "English" is a language picker with one language; it opens settings.
    app.click("launch.language");
    REQUIRE(app.wait_for([&]() { return app.screen() == "settings"; }));
    app.go("launch");
}
