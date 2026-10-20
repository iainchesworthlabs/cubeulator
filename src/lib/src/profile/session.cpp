#include <cube/profile/session.hpp>

#include <cube/profiling.hpp>

#include <algorithm>
#include <cstdint>

namespace cube::profile {

namespace {

// CMake passes the build's default as a compile definition of this file alone
// (a generator expression on the configuration), so no #ifdef decides it.
constexpr bool kBuildDefaultOn = CUBE_PROFILE_DEFAULT_ON != 0;

enum class parsed : std::uint8_t { unrecognised, on, off };

char lower_case(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// `word` is lower case already.
bool equals_ignoring_case(std::string_view value, std::string_view word) noexcept {
    return std::ranges::equal(value, word,
                              [](char a, char b) noexcept { return lower_case(a) == b; });
}

parsed parse_switch(std::string_view value) noexcept {
    for (const std::string_view word : {"1", "true", "on", "yes"}) {
        if (equals_ignoring_case(value, word)) {
            return parsed::on;
        }
    }
    for (const std::string_view word : {"0", "false", "off", "no"}) {
        if (equals_ignoring_case(value, word)) {
            return parsed::off;
        }
    }
    return parsed::unrecognised;
}

}  // namespace

bool build_default_on() noexcept {
    return kBuildDefaultOn;
}

startup_decision decide_startup(bool default_on, const char* environment,
                                std::span<const std::string_view> arguments) noexcept {
    startup_decision decision{.enabled = default_on, .source = switch_source::build_default};

    if (environment != nullptr) {
        switch (parse_switch(environment)) {
            case parsed::on:
                decision = {.enabled = true, .source = switch_source::environment};
                break;
            case parsed::off:
                decision = {.enabled = false, .source = switch_source::environment};
                break;
            case parsed::unrecognised:
                break;
        }
    }

    // The command line outranks the environment; the last flag outranks the
    // ones before it.
    for (const std::string_view argument : arguments) {
        if (argument == enable_flag) {
            decision = {.enabled = true, .source = switch_source::command_line};
        } else if (argument == disable_flag) {
            decision = {.enabled = false, .source = switch_source::command_line};
        }
    }
    return decision;
}

std::vector<std::string_view> arguments_of(int argc, const char* const* argv) {
    std::vector<std::string_view> arguments;
    for (int i = 1; i < argc; ++i) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic) - argv is the C array.
        arguments.emplace_back(argv[i]);
    }
    return arguments;
}

Session::Session(startup_decision decision) noexcept : decision_(decision) {
    if (decision_.enabled && cube::profiling::enabled && !cube::profiling::to_recorder) {
        cube::profiling::start();
        running_ = cube::profiling::active();
    }
}

Session::~Session() {
    if (running_) {
        cube::profiling::stop();
    }
}

}  // namespace cube::profile
