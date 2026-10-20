#pragma once

// The run-time switch for the Tracy profiler: whether this process starts it.
//
// A build with Tracy in it (CUBEULATOR_ENABLE_TRACY, on by default on the
// desktop) has the client linked but dormant - the overlay tracy port is built
// for manual lifetime, so nothing starts, listens or broadcasts until
// cube::profiling::start() runs (cube/profiling.hpp). This is the one place
// that decides whether it does:
//
//   1. the command line:  --profile  /  --no-profile  (the last one wins)
//   2. the environment:   CUBEULATOR_PROFILE = 1 | true | on | yes   (start)
//                                              0 | false | off | no  (do not)
//   3. the build's default: on in a Debug build, off in every other (the
//      CUBEULATOR_TRACY_AUTOSTART CMake option; docs/profiling.md)
//
// so a Debug build profiles without being asked and a Release build never does
// unless someone sets the variable or passes the flag - no recompile, and a
// shipped binary that is not asked behaves as one built without Tracy.
//
// The decision is a pure function so it is unit-tested without starting
// anything; Session is the RAII object main() holds for the process's life.

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace cube::profile {

// Where the decision came from, so the application can say so in its log.
enum class switch_source : std::uint8_t { build_default, environment, command_line };

struct startup_decision {
    bool enabled = false;
    switch_source source = switch_source::build_default;

    friend bool operator==(const startup_decision&, const startup_decision&) = default;
};

// The environment variable and the two flags, as the documentation spells them.
inline constexpr char environment_variable[] = "CUBEULATOR_PROFILE";
inline constexpr std::string_view enable_flag = "--profile";
inline constexpr std::string_view disable_flag = "--no-profile";

// What the build was configured to do when nobody says: true in a Debug
// build (CUBEULATOR_TRACY_AUTOSTART=AUTO), or whatever the option forced.
[[nodiscard]] bool build_default_on() noexcept;

// `environment` is the variable's value, or nullptr when it is unset;
// `arguments` is argv[1..]. An empty or unrecognised value is ignored, so a
// typo falls back to the build's default and does not silently start (or stop)
// a profiler.
[[nodiscard]] startup_decision decide_startup(bool default_on, const char* environment,
                                              std::span<const std::string_view> arguments) noexcept;

// argv[1..] as the views decide_startup takes. The environment is read by the
// caller (Qt's qgetenv in the application, std::getenv in a test binary), so
// that this library has no <cstdlib> getenv for MSVC to call unsafe.
[[nodiscard]] std::vector<std::string_view> arguments_of(int argc, const char* const* argv);

// Holds the profiler for the life of the process: starts it if the decision is
// to, and flushes and stops it at the end. Declare it first in main() so it is
// destroyed after every worker thread has been joined (no thread can then be
// inside a zone when the client is torn down).
class Session {
public:
    explicit Session(startup_decision decision) noexcept;
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    // The profiler is running (decided to, and this build has one to start).
    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] const startup_decision& decision() const noexcept { return decision_; }

private:
    startup_decision decision_;
    bool running_ = false;
};

}  // namespace cube::profile
