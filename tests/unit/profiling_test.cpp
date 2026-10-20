#include <catch2/catch_test_macros.hpp>

#include <cube/profile/qsg_timing.hpp>
#include <cube/profile/session.hpp>
#include <cube/profiling.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

// CUBE_EXPECT_TRACY and CUBE_EXPECT_RECORDER are set by tests/CMakeLists.txt
// from CUBEULATOR_ENABLE_TRACY and CUBEULATOR_ENABLE_PROFILE_RECORDER (1 or 0),
// so this file can ask "which variant did CMake select?" without an #ifdef of
// its own.

namespace {

// Every macro, in the shapes the sources use. In the disabled variant they all
// expand to nothing, so what this proves there is that a call site still
// compiles warning-free under -Werror; in the enabled variant it also runs
// the real Tracy client with nobody connected (on-demand mode), which must be
// harmless.
int instrumented_sum(int count) {
    CUBE_ZONE_SCOPED();
    int total = 0;
    for (int i = 0; i < count; ++i) {
        CUBE_ZONE_SCOPED_N("test.iteration");
        total += i;
    }
    return total;
}

int manual_zone(int count) {
    CUBE_ZONE_BEGIN(span, "test.manual");
    const int total = instrumented_sum(count);
    CUBE_ZONE_VALUE(span, count);
    CUBE_ZONE_END(span);
    return total;
}

}  // namespace

TEST_CASE("the profiling variant matches the CMake options", "[profiling]") {
    REQUIRE(cube::profiling::enabled == (CUBE_EXPECT_TRACY != 0 || CUBE_EXPECT_RECORDER != 0));
    REQUIRE(cube::profiling::to_recorder == (CUBE_EXPECT_RECORDER != 0));
}

TEST_CASE("instrumented code computes the same result", "[profiling]") {
    REQUIRE(instrumented_sum(5) == 10);
    REQUIRE(manual_zone(5) == 10);
}

TEST_CASE("the process-level macros compile and run", "[profiling]") {
    CUBE_THREAD_NAME("cube-unit-tests");
    CUBE_APP_INFO(std::string_view{"cube_unit_tests"});
    CUBE_MESSAGE("test.message");

    // One of each argument type the sources plot: a count, a float, a flag.
    // [[maybe_unused]] because the disabled variant's macros never read them.
    [[maybe_unused]] const std::int64_t count = 7;
    [[maybe_unused]] const float ratio = 0.5F;
    [[maybe_unused]] const bool flag = true;
    CUBE_PLOT("test.count", count);
    CUBE_PLOT("test.ratio", ratio);
    CUBE_PLOT("test.flag", flag);

    CUBE_FRAME_MARK();
    CUBE_FRAME_MARK_NAMED("test.frames");

    // Execution sections (Tracy 0.14), in the shape the app uses them: the
    // id is kept across the region and handed back to leave.
    const std::uint32_t section = CUBE_SECTION_ENTER("test.section");
    CUBE_SECTION_LEAVE(section);

    // Nothing to assert about the instrumentation itself - it has no
    // observable effect on the program - beyond having got this far.
    SUCCEED();
}

TEST_CASE("the GPU profiling surface exists and rejects empty handles", "[profiling]") {
    // The render thread's two tools compile in every variant, and the real
    // one refuses to profile without the handles a window supplies.
    ::cube::profiling::ZoneSpan span;
    span.begin("test.span");
    span.end();
    span.end();  // idempotent, like the zones themselves

    REQUIRE(::cube::profiling::GpuContext::create_opengl({}) == nullptr);
    REQUIRE(::cube::profiling::GpuContext::create_d3d11(nullptr, nullptr) == nullptr);
    REQUIRE(::cube::profiling::GpuContext::create_vulkan({}) == nullptr);
}

namespace {

using cube::profile::decide_startup;
using cube::profile::startup_decision;
using cube::profile::switch_source;

startup_decision decide(bool default_on, const char* environment,
                        std::span<const std::string_view> arguments = {}) {
    return decide_startup(default_on, environment, arguments);
}

}  // namespace

TEST_CASE("the profiling switch falls back to the build's default", "[profiling][session]") {
    REQUIRE(decide(true, nullptr) ==
            startup_decision{.enabled = true, .source = switch_source::build_default});
    REQUIRE(decide(false, nullptr) ==
            startup_decision{.enabled = false, .source = switch_source::build_default});
}

TEST_CASE("CUBEULATOR_PROFILE switches the profiler on and off", "[profiling][session]") {
    for (const char* on : {"1", "true", "TRUE", "On", "yes"}) {
        REQUIRE(decide(false, on) ==
                startup_decision{.enabled = true, .source = switch_source::environment});
    }
    for (const char* off : {"0", "false", "Off", "NO"}) {
        REQUIRE(decide(true, off) ==
                startup_decision{.enabled = false, .source = switch_source::environment});
    }
    // A value that means neither, or none at all, changes nothing: a typo
    // must not silently start (or stop) a profiler that listens on a port.
    for (const char* unclear : {"", "2", "maybe", "enable"}) {
        REQUIRE(decide(false, unclear).source == switch_source::build_default);
        REQUIRE(decide(true, unclear).enabled);
    }
}

TEST_CASE("the command line outranks the environment, and the last flag wins",
          "[profiling][session]") {
    constexpr std::array<std::string_view, 1> profile{"--profile"};
    constexpr std::array<std::string_view, 1> no_profile{"--no-profile"};
    constexpr std::array<std::string_view, 3> flip{"--profile", "--no-profile", "--profile"};
    constexpr std::array<std::string_view, 2> unrelated{"--other", "profile"};

    REQUIRE(decide(false, "0", profile) ==
            startup_decision{.enabled = true, .source = switch_source::command_line});
    REQUIRE(decide(true, "1", no_profile) ==
            startup_decision{.enabled = false, .source = switch_source::command_line});
    REQUIRE(decide(false, nullptr, flip).enabled);
    // Only the two flags, spelled exactly, count.
    REQUIRE(decide(false, nullptr, unrelated) ==
            startup_decision{.enabled = false, .source = switch_source::build_default});
}

TEST_CASE("arguments_of skips the program name", "[profiling][session]") {
    const std::array<const char*, 3> argv{"Cubeulator", "--profile", "x"};
    const auto arguments = cube::profile::arguments_of(3, argv.data());
    REQUIRE(arguments.size() == 2);
    REQUIRE(arguments.at(0) == "--profile");
    REQUIRE(cube::profile::arguments_of(0, nullptr).empty());
}

TEST_CASE("the build default follows CMake's choice", "[profiling][session]") {
    // CUBE_EXPECT_PROFILE_DEFAULT is the generator expression CMake gave
    // session.cpp, evaluated again for this target.
    REQUIRE(cube::profile::build_default_on() == (CUBE_EXPECT_PROFILE_DEFAULT != 0));
}

TEST_CASE("a process that never starts the profiler leaves every macro inert",
          "[profiling][session]") {
    // This test binary never builds a running Session unless the environment
    // asks (the perf listener does), so Tracy's manual-lifetime state must not
    // exist and nothing below may touch it.
    if (cube::profiling::enabled && !cube::profiling::to_recorder) {
        REQUIRE_FALSE(cube::profiling::active());
    }
    CUBE_ZONE_SCOPED_N("test.inert");
    CUBE_PLOT("test.inert_plot", 1);
    CUBE_FRAME_MARK_NAMED("test.inert_frames");
    CUBE_MESSAGE("test.inert_message");
    CUBE_THREAD_NAME("inert");
    const std::uint32_t section = CUBE_SECTION_ENTER("test.inert_section");
    CUBE_SECTION_LEAVE(section);
    cube::profile::Session session(startup_decision{.enabled = false});
    REQUIRE_FALSE(session.running());
}

namespace {

using cube::profile::parse_qsg_timing;

double sample_of(const cube::profile::qsg_line& line, std::string_view plot) {
    for (const auto& sample : line.samples) {
        if (plot == sample.plot) {
            return sample.value;
        }
    }
    return -1.0;
}

}  // namespace

// The lines below are copied from a run of the application (Qt 6.12, threaded
// render loop), so the reading is pinned to what Qt really prints.
TEST_CASE("the scene graph's timing lines become plots", "[profiling][qsg]") {
    REQUIRE(cube::profile::is_qsg_timing_category("qt.scenegraph.time.renderer"));
    REQUIRE_FALSE(cube::profile::is_qsg_timing_category("qt.scenegraph.general"));

    const auto frame = parse_qsg_timing(
        {.category = "qt.scenegraph.time.renderloop",
         .text = "[window 0x1ad5a6e87f0][render thread 0x1ad5befdf50] syncAndRender: frame "
                 "rendered in 12ms, sync=3, render=4, swap=5"});
    REQUIRE(sample_of(frame, "qsg.sync_ms") == 3.0);
    REQUIRE(sample_of(frame, "qsg.render_ms") == 4.0);
    REQUIRE(sample_of(frame, "qsg.swap_ms") == 5.0);
    REQUIRE_FALSE(frame.as_message);

    // The basic loop's wording, with a polish phase.
    const auto basic = parse_qsg_timing(
        {.category = "qt.scenegraph.time.renderloop",
         .text = "Frame rendered with 'basic' renderloop in 4ms, polish=0, sync=1, render=2, "
                 "swap=1, frameDelta=16"});
    REQUIRE(sample_of(basic, "qsg.polish_ms") == 0.0);
    REQUIRE(sample_of(basic, "qsg.swap_ms") == 1.0);

    // A frame boundary line carries no phase times.
    REQUIRE(
        parse_qsg_timing({.category = "qt.scenegraph.time.renderloop",
                          .text = "[window 0x1][gui thread] polishAndSync: start, elapsed since "
                                  "last call: 0 ms"})
            .samples.empty());

    const auto renderer = parse_qsg_timing(
        {.category = "qt.scenegraph.time.renderer",
         .text = "time in renderer: total=144ms, preprocess=143, updates=0, rendering=1"});
    REQUIRE(sample_of(renderer, "qsg.renderer_ms") == 144.0);
    REQUIRE(sample_of(renderer, "qsg.renderer_preprocess_ms") == 143.0);
    REQUIRE(sample_of(renderer, "qsg.renderer_rendering_ms") == 1.0);

    const auto glyphs = parse_qsg_timing(
        {.category = "qt.scenegraph.time.glyph",
         .text = "distancefield: 38 glyphs prepared in 7ms, rendering=2, upload=4"});
    REQUIRE(sample_of(glyphs, "qsg.glyph_ms") == 7.0);
    REQUIRE(sample_of(glyphs, "qsg.glyph_upload_ms") == 4.0);
}

TEST_CASE("texture and shader lines are kept as messages", "[profiling][qsg]") {
    const auto texture = parse_qsg_timing({.category = "qt.scenegraph.time.texture",
                                           .text = "plain texture uploaded: total=6ms, upload=2"});
    REQUIRE(texture.as_message);
    REQUIRE(sample_of(texture, "qsg.texture_ms") == 6.0);
    REQUIRE(parse_qsg_timing(
                {.category = "qt.scenegraph.time.compilation", .text = "shader compiled in 9 ms"})
                .as_message);
    // An unknown category or an unreadable line is not an error: nothing to plot.
    REQUIRE(parse_qsg_timing({.category = "qt.scenegraph.time.other", .text = "anything=4"})
                .samples.empty());
    REQUIRE(parse_qsg_timing(
                {.category = "qt.scenegraph.time.renderer", .text = "total=ms, preprocess="})
                .samples.empty());
    // A key must start a word: "prerender=" is not "render=".
    REQUIRE(parse_qsg_timing(
                {.category = "qt.scenegraph.time.renderloop", .text = "frame rendered prerender=9"})
                .samples.empty());
}
