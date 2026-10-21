#pragma once

// Which graphics API the window renders with, and what happens when it cannot.
//
// The API is fixed for the life of a process (QQuickWindow::setGraphicsApi()
// only counts before the first window), so it is chosen once, at launch, from
// the places a person or a developer can say so, and a launch that could not
// use it must not be able to leave the application unable to start. Nothing
// here touches Qt or the OS: the choice is a pure function of its inputs so it
// is unit-tested without a window, and src/app/desktop/graphics_backend.cpp is
// the one place that reads the environment, applies the answer and remembers
// failures. docs/graphics-api.md is the explanation.
//
// A backend is named by a key: "d3d11", "d3d12", "vulkan", "opengl", "metal",
// or "software" (the platform's own software rasteriser; Windows draws with
// Direct3D 11 on WARP). "auto" is not a backend: it means "the platform's
// chain", below.
//
// Where the choice comes from, strongest first:
//
//   1. the command line:  --graphics-api=<key>      (the last one wins)
//   2. the environment:   CUBEULATOR_GRAPHICS_API=<key>
//   3. Qt's own variable: QSG_RHI_BACKEND=<key>     (what a Qt developer reaches for)
//   4. the Settings screen's picker (rendering/graphicsBackend, persisted)
//   5. the platform's automatic chain
//
// "auto" at 1 to 3 stops the search there: it is how a person overrides the
// picker for one launch. A key the platform does not offer (a typo, or d3d11
// on Linux) is ignored with a note, never obeyed: asking for what cannot work
// must not be what stops the application starting.
//
// The chain. A platform lists the backends "auto" tries in order, best first
// (Windows: Direct3D 11, then software). A request names the first backend to
// try; the chain follows it, without repeating anything. The caller starts the
// first entry of `choice::chain`; if the window cannot be created with it - or
// a previous launch with it never got as far as a frame - the next entry is
// used. The last entry of the platform's chain is the floor: no record of
// failure removes it, because there is nothing below it.
//
// The guard remembers failures across launches (`record`):
//
//   * `pending` is the backend a launch set out with; it is cleared when a
//     frame has been presented or the application quits normally. If the next
//     launch finds it still set, the last one crashed, hung or was killed
//     before it could draw. That is a strike against the backend
//     (`suspect`), and the second strike in a row enters it in `failed`: one
//     is not enough, because a second copy of the application started while
//     the first is still opening its window looks the same;
//   * `failed` backends are skipped by every launch that did not ask for them
//     by name on the command line or in CUBEULATOR_GRAPHICS_API. A failure
//     lapses when the application is a different build (a new Qt, a new
//     commit), after `retry_after_seconds` (a driver update is the usual
//     cure), or when the person changes the Settings picker (they have said
//     what they want, so it is tried); the first frame a backend draws
//     removes it.

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cube::graphics {

inline constexpr std::string_view environment_variable = "CUBEULATOR_GRAPHICS_API";
inline constexpr std::string_view qt_environment_variable = "QSG_RHI_BACKEND";
inline constexpr std::string_view automatic = "auto";
inline constexpr std::string_view software = "software";

// --graphics-api=<key> (or `--graphics-api <key>`), and the flag the
// application adds to its own relaunch so a backend that just failed in this
// chain is not picked again even when it was asked for by name.
inline constexpr std::string_view option_graphics_api = "--graphics-api";
inline constexpr std::string_view option_skip = "--graphics-skip";

// How long a recorded failure keeps a backend out, unless the build changes
// first: a week.
inline constexpr std::int64_t retry_after_seconds = 7LL * 24 * 60 * 60;

enum class source : std::uint8_t {
    platform_chain,  // nobody asked: the platform's automatic chain
    setting,         // the Settings screen's picker
    qt_environment,  // QSG_RHI_BACKEND
    environment,     // CUBEULATOR_GRAPHICS_API
    command_line,    // --graphics-api
};

[[nodiscard]] std::string_view source_name(source s) noexcept;

// Everything a request can say, read by the caller.
struct request {
    std::optional<std::string> command_line;
    std::optional<std::string> environment;
    std::optional<std::string> qt_environment;
    std::string setting{automatic};
    // Backends that already failed in this relaunch chain.
    std::vector<std::string> skip;
};

// What the platform offers (interfaces/platform_info.hpp supplies it).
struct platform {
    std::vector<std::string> available;  // every key this platform can apply
    std::vector<std::string>
        automatic;  // the chain "auto" tries, best first; empty: leave it to Qt
};

// One backend that failed, and the build, the Settings picker's value and the
// moment it happened on.
struct failure {
    std::string backend;
    std::string build;
    std::int64_t when = 0;  // seconds since the epoch
    std::string setting{automatic};

    friend bool operator==(const failure&, const failure&) = default;
};

// What is remembered between launches.
struct record {
    std::string pending;
    std::string suspect;  // the backend with a strike against it ...
    int strikes = 0;      // ... and how many launches in a row it has not outlived
    std::vector<failure> failed;

    friend bool operator==(const record&, const record&) = default;
};

struct choice {
    // The backends to try, first one first. Empty means "set nothing": Qt picks
    // its own default (a platform with no chain, or every backend out).
    std::vector<std::string> chain;
    source from = source::platform_chain;
    // The backends a request or the picker named but that were set aside, and
    // why, one line each, for the application's log.
    std::vector<std::string> notes;
    // The guard applies: the platform has a chain, so a failed launch has
    // somewhere to fall back to.
    bool guarded = false;
};

// What the command line carries.
struct arguments {
    std::optional<std::string> graphics_api;
    std::vector<std::string> skip;
};

[[nodiscard]] arguments parse_arguments(std::span<const std::string_view> argv_tail);

// argv for the relaunch after `failed` could not be used: the original
// arguments, without an earlier --graphics-skip, and with this chain's skips
// (`already` and `failed`) in one.
[[nodiscard]] std::vector<std::string> relaunch_arguments(
    std::span<const std::string_view> argv_tail, std::span<const std::string> already,
    std::string_view failed);

// The failures of this record that still count at `now` on `build`, with the
// picker at `setting`.
[[nodiscard]] std::vector<std::string> active_failures(const record& r, std::string_view build,
                                                       std::int64_t now, std::string_view setting);

// Strikes in a row that make a failure of a backend that never presented a frame.
inline constexpr int strikes_to_fail = 2;

// Step 1 of a launch: a `pending` left behind is a launch that never drew. It
// is a strike, and the `strikes_to_fail`th in a row is a failure (stamped with
// `build` and `now`). Failures that no longer count are dropped.
[[nodiscard]] record account_for_previous_launch(const record& r, std::string_view build,
                                                 std::int64_t now, std::string_view setting);

// Step 2: the choice. `r` is the record from step 1.
[[nodiscard]] choice decide(const request& asked, const platform& offered, const record& r,
                            std::string_view build, std::int64_t now);

// Step 3: the launch sets out with `backend`.
[[nodiscard]] record launch_started(const record& r, std::string_view backend);

// Step 4: a frame was presented with `backend`: it works, so it is not pending,
// has no strike and no failure on file.
[[nodiscard]] record launch_succeeded(const record& r, std::string_view backend);

// The application quit normally before it drew anything (a person closed the
// window while it opened): not a crash, so `backend` is no longer pending, but
// it has proved nothing either.
[[nodiscard]] record launch_ended(const record& r, std::string_view backend);

// The window could not be created with `backend` (reported, not crashed):
// recorded at once, so the relaunch and every later launch go past it.
[[nodiscard]] record launch_failed(const record& r, std::string_view backend,
                                   std::string_view build, std::int64_t now,
                                   std::string_view setting);

}  // namespace cube::graphics
