// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <cube/graphics/backend_choice.hpp>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

using cube::graphics::choice;
using cube::graphics::decide;
using cube::graphics::failure;
using cube::graphics::platform;
using cube::graphics::record;
using cube::graphics::request;
using cube::graphics::source;

constexpr std::string_view kBuild = "0.2.0-beta.1+qt6.12";
constexpr std::int64_t kNow = 1'800'000'000;
constexpr std::int64_t kDay = std::int64_t{24} * 60 * 60;

using keys = std::vector<std::string>;

// Windows as src/app/platform/windows/platform_info_windows.cpp describes it.
platform windows() {
    return {
        .available = {"d3d11", "d3d12", "vulkan", "opengl", "software"},
        .automatic = {"d3d11", "software"},
    };
}

// A platform with no chain: Qt picks its own, nothing is guarded.
platform unmanaged() {
    return {.available = {"opengl", "vulkan"}, .automatic = keys()};
}

failure failed(std::string backend, std::int64_t when = kNow - kDay,
               std::string build = std::string(kBuild), std::string setting = "auto") {
    return {
        .backend = std::move(backend),
        .build = std::move(build),
        .when = when,
        .setting = std::move(setting),
    };
}

}  // namespace

TEST_CASE("nobody asks: the platform's chain, guarded", "[graphics]") {
    const choice c = decide(request(), windows(), record(), kBuild, kNow);
    CHECK(c.chain == keys{"d3d11", "software"});
    CHECK(c.from == source::platform_chain);
    CHECK(c.guarded);
    CHECK(c.notes.empty());
}

TEST_CASE("a platform with no chain leaves the choice to Qt and guards nothing", "[graphics]") {
    const choice c = decide(request(), unmanaged(), record(), kBuild, kNow);
    CHECK(c.chain.empty());
    CHECK_FALSE(c.guarded);
}

TEST_CASE("the picker's backend goes first and the chain follows it", "[graphics]") {
    request asked;
    asked.setting = "vulkan";
    const choice c = decide(asked, windows(), record(), kBuild, kNow);
    CHECK(c.chain == keys{"vulkan", "d3d11", "software"});
    CHECK(c.from == source::setting);
}

TEST_CASE("the backend asked for is not repeated by the chain", "[graphics]") {
    request asked;
    asked.setting = "d3d11";
    CHECK(decide(asked, windows(), record(), kBuild, kNow).chain == keys{"d3d11", "software"});

    asked.setting = "software";
    CHECK(decide(asked, windows(), record(), kBuild, kNow).chain == keys{"software", "d3d11"});
}

TEST_CASE("sources are read strongest first", "[graphics]") {
    request asked;
    asked.setting = "opengl";
    asked.qt_environment = "d3d12";
    CHECK(decide(asked, windows(), record(), kBuild, kNow).chain.front() == "d3d12");
    CHECK(decide(asked, windows(), record(), kBuild, kNow).from == source::qt_environment);

    asked.environment = "vulkan";
    CHECK(decide(asked, windows(), record(), kBuild, kNow).chain.front() == "vulkan");
    CHECK(decide(asked, windows(), record(), kBuild, kNow).from == source::environment);

    asked.command_line = "software";
    CHECK(decide(asked, windows(), record(), kBuild, kNow).chain.front() == "software");
    CHECK(decide(asked, windows(), record(), kBuild, kNow).from == source::command_line);
}

TEST_CASE("any case and stray spaces still name a backend", "[graphics]") {
    request asked;
    asked.environment = "  Vulkan ";
    CHECK(decide(asked, windows(), record(), kBuild, kNow).chain.front() == "vulkan");
}

TEST_CASE("auto stops the search: it is how one launch overrides the picker", "[graphics]") {
    request asked;
    asked.setting = "vulkan";
    asked.command_line = "auto";
    const choice c = decide(asked, windows(), record(), kBuild, kNow);
    CHECK(c.chain == keys{"d3d11", "software"});
    CHECK(c.from == source::command_line);
}

TEST_CASE("a key the platform does not offer is set aside, never obeyed", "[graphics]") {
    request asked;
    asked.command_line = "metal";
    asked.setting = "vulkan";
    const choice c = decide(asked, windows(), record(), kBuild, kNow);
    // The next source down is read instead.
    CHECK(c.chain == keys{"vulkan", "d3d11", "software"});
    CHECK(c.from == source::setting);
    REQUIRE(c.notes.size() == 1);
    CHECK(c.notes.front().contains("metal"));

    request typo;
    typo.environment = "vulcan";
    const choice t = decide(typo, windows(), record(), kBuild, kNow);
    CHECK(t.chain == keys{"d3d11", "software"});
    CHECK(t.notes.size() == 1);
}

TEST_CASE("a failed backend is skipped unless it was named on the command line", "[graphics]") {
    record on_picker;
    on_picker.failed = {failed("vulkan", kNow - kDay, std::string(kBuild), "vulkan")};
    request picker;
    picker.setting = "vulkan";
    const choice via_picker = decide(picker, windows(), on_picker, kBuild, kNow);
    CHECK(via_picker.chain == keys{"d3d11", "software"});
    CHECK(via_picker.notes.size() == 1);

    record r;
    r.failed = {failed("vulkan")};

    request qt;
    qt.qt_environment = "vulkan";
    CHECK(decide(qt, windows(), r, kBuild, kNow).chain == keys{"d3d11", "software"});

    request by_name;
    by_name.command_line = "vulkan";
    CHECK(decide(by_name, windows(), r, kBuild, kNow).chain == keys{"vulkan", "d3d11", "software"});

    request by_environment;
    by_environment.environment = "vulkan";
    CHECK(decide(by_environment, windows(), r, kBuild, kNow).chain ==
          keys{"vulkan", "d3d11", "software"});
}

TEST_CASE("a failure of the platform's default sends the launch down the chain", "[graphics]") {
    record r;
    r.failed = {failed("d3d11")};
    CHECK(decide(request(), windows(), r, kBuild, kNow).chain == keys{"software"});
}

TEST_CASE("the last of the chain is the floor: no failure removes it", "[graphics]") {
    record r;
    r.failed = {failed("d3d11"), failed("software")};
    CHECK(decide(request(), windows(), r, kBuild, kNow).chain == keys{"software"});
}

TEST_CASE("a relaunch never picks what just failed, even when it was named", "[graphics]") {
    request asked;
    asked.command_line = "vulkan";
    asked.skip = {"vulkan"};
    CHECK(decide(asked, windows(), record(), kBuild, kNow).chain == keys{"d3d11", "software"});

    asked.skip = {"vulkan", "d3d11", "software"};
    CHECK(decide(asked, windows(), record(), kBuild, kNow).chain.empty());
}

TEST_CASE("failures of another build, or older than a week, do not count", "[graphics]") {
    record r;
    r.failed = {
        failed("vulkan", kNow - kDay, "0.1.0"),
        failed("opengl", kNow - (8 * kDay)),
        failed("d3d12", kNow - (6 * kDay)),
    };
    CHECK(cube::graphics::active_failures(r, kBuild, kNow, "auto") == keys{"d3d12"});

    request asked;
    asked.setting = "vulkan";
    CHECK(decide(asked, windows(), r, kBuild, kNow).chain.front() == "vulkan");
}

TEST_CASE("a launch that never drew is a strike; two in a row are a failure", "[graphics]") {
    record r = cube::graphics::launch_started(record(), "vulkan");
    CHECK(r.pending == "vulkan");

    r = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "auto");
    CHECK(r.pending.empty());
    CHECK(r.suspect == "vulkan");
    CHECK(r.strikes == 1);
    CHECK(r.failed.empty());

    r = cube::graphics::launch_started(r, "vulkan");
    r = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "auto");
    CHECK(r.strikes == 0);
    CHECK(r.suspect.empty());
    REQUIRE(r.failed.size() == 1);
    CHECK(r.failed.front().backend == "vulkan");
    CHECK(r.failed.front().build == kBuild);
    CHECK(r.failed.front().when == kNow);
}

TEST_CASE("a launch that drew a frame clears the strike", "[graphics]") {
    record r = cube::graphics::launch_started(record(), "vulkan");
    r = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "auto");
    REQUIRE(r.strikes == 1);

    r = cube::graphics::launch_started(r, "vulkan");
    r = cube::graphics::launch_succeeded(r, "vulkan");
    CHECK(r.pending.empty());
    CHECK(r.strikes == 0);
    CHECK(r.suspect.empty());

    // So the next unfinished launch is a first strike again.
    r = cube::graphics::launch_started(r, "vulkan");
    r = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "auto");
    CHECK(r.strikes == 1);
    CHECK(r.failed.empty());
}

TEST_CASE("strikes against different backends do not add up", "[graphics]") {
    record r = cube::graphics::launch_started(record(), "vulkan");
    r = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "auto");
    r = cube::graphics::launch_started(r, "d3d11");
    r = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "auto");
    CHECK(r.suspect == "d3d11");
    CHECK(r.strikes == 1);
    CHECK(r.failed.empty());
}

TEST_CASE("a normal quit before the first frame is not a strike", "[graphics]") {
    record r = cube::graphics::launch_started(record(), "d3d11");
    r = cube::graphics::launch_ended(r, "d3d11");
    CHECK(r.pending.empty());
    r = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "auto");
    CHECK(r.strikes == 0);
    CHECK(r.failed.empty());
}

TEST_CASE("a window that could not be created is a failure at once", "[graphics]") {
    record r = cube::graphics::launch_started(record(), "vulkan");
    r = cube::graphics::launch_failed(r, "vulkan", kBuild, kNow, "vulkan");
    CHECK(r.pending.empty());
    REQUIRE(r.failed.size() == 1);
    CHECK(r.failed.front().backend == "vulkan");

    // And it is skipped by the next launch without anyone naming it.
    request asked;
    asked.setting = "vulkan";
    CHECK(decide(asked, windows(), r, kBuild, kNow).chain == keys{"d3d11", "software"});
}

TEST_CASE("a backend that draws a frame is forgiven", "[graphics]") {
    record r;
    r.failed = {failed("vulkan"), failed("opengl")};
    r = cube::graphics::launch_succeeded(r, "vulkan");
    REQUIRE(r.failed.size() == 1);
    CHECK(r.failed.front().backend == "opengl");
}

TEST_CASE("a failure is recorded once, with its latest time", "[graphics]") {
    record r;
    r = cube::graphics::launch_failed(r, "vulkan", kBuild, kNow - kDay, "auto");
    r = cube::graphics::launch_failed(r, "vulkan", kBuild, kNow, "auto");
    REQUIRE(r.failed.size() == 1);
    CHECK(r.failed.front().when == kNow);
}

TEST_CASE("stale failures are dropped when the next launch accounts for the last", "[graphics]") {
    record r;
    r.failed = {failed("opengl", kNow - (9 * kDay)), failed("d3d12", kNow - kDay)};
    r = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "auto");
    REQUIRE(r.failed.size() == 1);
    CHECK(r.failed.front().backend == "d3d12");
}

TEST_CASE("the command line is read both ways round", "[graphics]") {
    using namespace std::string_view_literals;
    const std::array<std::string_view, 3> joined{
        "--profile"sv,
        "--graphics-api=Vulkan"sv,
        "--graphics-skip=d3d11, software"sv,
    };
    const auto a = cube::graphics::parse_arguments(joined);
    CHECK(a.graphics_api == "vulkan");
    CHECK(a.skip == keys{"d3d11", "software"});

    const std::array<std::string_view, 2> spaced{"--graphics-api"sv, "software"sv};
    CHECK(cube::graphics::parse_arguments(spaced).graphics_api == "software");

    const std::array<std::string_view, 2> last_wins{
        "--graphics-api=vulkan"sv,
        "--graphics-api=opengl"sv,
    };
    CHECK(cube::graphics::parse_arguments(last_wins).graphics_api == "opengl");

    const std::array<std::string_view, 1> dangling{"--graphics-api"sv};
    CHECK_FALSE(cube::graphics::parse_arguments(dangling).graphics_api.has_value());

    const std::array<std::string_view, 1> unrelated{"--profile"sv};
    const auto none = cube::graphics::parse_arguments(unrelated);
    CHECK_FALSE(none.graphics_api.has_value());
    CHECK(none.skip.empty());
}

TEST_CASE("a relaunch carries its arguments and every backend skipped so far", "[graphics]") {
    using namespace std::string_view_literals;
    const std::array<std::string_view, 3> original{
        "--profile"sv,
        "--graphics-api=vulkan"sv,
        "--graphics-skip=opengl"sv,
    };
    const keys already{"opengl"};
    const auto out = cube::graphics::relaunch_arguments(original, already, "vulkan");
    CHECK(out == keys{"--profile", "--graphics-api=vulkan", "--graphics-skip=opengl,vulkan"});

    const auto again = cube::graphics::relaunch_arguments(original, already, "opengl");
    CHECK(again.back() == "--graphics-skip=opengl");

    const std::vector<std::string_view> no_arguments;
    const keys no_skips;
    const auto first = cube::graphics::relaunch_arguments(no_arguments, no_skips, "vulkan");
    CHECK(first == keys{"--graphics-skip=vulkan"});
}

TEST_CASE("choosing again in the Settings picker forgives the failures recorded before",
          "[graphics]") {
    // vulkan failed while the picker said "auto" ...
    record r;
    r.failed = {failed("vulkan", kNow - kDay, std::string(kBuild), "auto")};

    // ... and the person then picked it by name: it is tried.
    request asked;
    asked.setting = "vulkan";
    CHECK(decide(asked, windows(), r, kBuild, kNow).chain.front() == "vulkan");

    // The next launch also forgets it, so it does not come back if they pick "auto" again.
    const record next = cube::graphics::account_for_previous_launch(r, kBuild, kNow, "vulkan");
    CHECK(next.failed.empty());
}

TEST_CASE("a failure remembers the picker it happened under", "[graphics]") {
    record r = cube::graphics::launch_failed(record(), "vulkan", kBuild, kNow, " Vulkan ");
    REQUIRE(r.failed.size() == 1);
    CHECK(r.failed.front().setting == "vulkan");

    request asked;
    asked.setting = "vulkan";
    CHECK(decide(asked, windows(), r, kBuild, kNow).chain == keys{"d3d11", "software"});
}
