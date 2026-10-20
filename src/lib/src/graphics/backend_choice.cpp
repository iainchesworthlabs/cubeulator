#include <cube/graphics/backend_choice.hpp>

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

namespace cube::graphics {

namespace {

std::string lower_case(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return out;
}

std::string trimmed(std::string_view text) {
    constexpr std::string_view kBlanks = " \t\r\n";
    const auto first = text.find_first_not_of(kBlanks);
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(kBlanks);
    return std::string(text.substr(first, last - first + 1));
}

// A key as a person types it: any case, spaces around it.
std::string normalised(std::string_view text) {
    return lower_case(trimmed(text));
}

bool contains(const std::vector<std::string>& keys, std::string_view key) {
    return std::ranges::find(keys, key) != keys.end();
}

void split_into(std::string_view list, std::vector<std::string>& out) {
    while (!list.empty()) {
        const auto comma = list.find(',');
        const std::string key = normalised(list.substr(0, comma));
        if (!key.empty() && !contains(out, key)) {
            out.push_back(key);
        }
        if (comma == std::string_view::npos) {
            break;
        }
        list.remove_prefix(comma + 1);
    }
}

// `--name=value`: the value, or nullopt when `argument` is not that option.
std::optional<std::string_view> value_of(std::string_view argument, std::string_view name) {
    if (!argument.starts_with(name)) {
        return std::nullopt;
    }
    const std::string_view rest = argument.substr(name.size());
    if (!rest.starts_with('=')) {
        return std::nullopt;
    }
    return rest.substr(1);
}

std::string_view describe(source from) noexcept {
    return source_name(from);
}

record without_failure(record r, std::string_view backend) {
    std::erase_if(r.failed, [&](const failure& f) { return f.backend == backend; });
    return r;
}

record with_failure(record r, std::string_view backend, std::string_view build, std::int64_t now,
                    std::string_view setting) {
    r = without_failure(std::move(r), backend);
    r.failed.push_back({
        .backend = std::string(backend),
        .build = std::string(build),
        .when = now,
        .setting = normalised(setting),
    });
    return r;
}

}  // namespace

std::string_view source_name(source s) noexcept {
    switch (s) {
        case source::platform_chain:
            return "the platform's automatic choice";
        case source::setting:
            return "the Settings screen";
        case source::qt_environment:
            return "QSG_RHI_BACKEND";
        case source::environment:
            return "CUBEULATOR_GRAPHICS_API";
        case source::command_line:
            return "--graphics-api";
    }
    return "";
}

arguments parse_arguments(std::span<const std::string_view> argv_tail) {
    arguments parsed;
    for (auto it = argv_tail.begin(); it != argv_tail.end(); ++it) {
        const std::string_view argument = *it;
        if (const auto value = value_of(argument, option_graphics_api)) {
            parsed.graphics_api = normalised(*value);
        } else if (argument == option_graphics_api && std::next(it) != argv_tail.end()) {
            ++it;
            parsed.graphics_api = normalised(*it);
        } else if (const auto skipped = value_of(argument, option_skip)) {
            split_into(*skipped, parsed.skip);
        }
    }
    return parsed;
}

std::vector<std::string> relaunch_arguments(std::span<const std::string_view> argv_tail,
                                            std::span<const std::string> already,
                                            std::string_view failed) {
    std::vector<std::string> out;
    for (const std::string_view argument : argv_tail) {
        if (!value_of(argument, option_skip)) {
            out.emplace_back(argument);
        }
    }
    std::string skips;
    for (const std::string& key : already) {
        skips += (skips.empty() ? "" : ",") + key;
    }
    if (!failed.empty() && std::ranges::find(already, failed) == already.end()) {
        skips += (skips.empty() ? "" : ",") + std::string(failed);
    }
    out.push_back(std::string(option_skip) + "=" + skips);
    return out;
}

std::vector<std::string> active_failures(const record& r, std::string_view build, std::int64_t now,
                                         std::string_view setting) {
    std::vector<std::string> keys;
    const std::string picker = normalised(setting);
    for (const failure& f : r.failed) {
        if (f.build == build && now - f.when < retry_after_seconds && f.setting == picker) {
            keys.push_back(f.backend);
        }
    }
    return keys;
}

record account_for_previous_launch(const record& r, std::string_view build, std::int64_t now,
                                   std::string_view setting) {
    record out = r;
    // A failure the person has since answered by choosing again is gone.
    const std::string picker = normalised(setting);
    std::erase_if(out.failed, [&](const failure& f) {
        return f.build != build || now - f.when >= retry_after_seconds || f.setting != picker;
    });
    if (out.pending.empty()) {
        return out;
    }
    const std::string crashed = std::exchange(out.pending, std::string());
    out.strikes = (out.suspect == crashed) ? out.strikes + 1 : 1;
    out.suspect = crashed;
    if (out.strikes >= strikes_to_fail) {
        out = with_failure(std::move(out), crashed, build, now, setting);
        out.suspect.clear();
        out.strikes = 0;
    }
    return out;
}

namespace {

// What was asked for, once the sources have been read.
struct asked_for {
    std::string key;  // empty: nothing, so the platform's chain
    source from = source::platform_chain;
    bool by_name = false;  // on the command line or in CUBEULATOR_GRAPHICS_API
};

// One place a backend can be asked for, its value as a person types it.
struct named {
    source from;
    std::string key;  // empty when that place says nothing
};

// The strongest source that names a backend this platform can apply. "auto"
// settles it at once; an unknown key is set aside with a note and the next
// source is read.
asked_for read_sources(const request& asked, const platform& offered,
                       std::vector<std::string>& notes) {
    const std::array<named, 4> sources{{
        {.from = source::command_line,
         .key = asked.command_line ? normalised(*asked.command_line) : std::string()},
        {.from = source::environment,
         .key = asked.environment ? normalised(*asked.environment) : std::string()},
        {.from = source::qt_environment,
         .key = asked.qt_environment ? normalised(*asked.qt_environment) : std::string()},
        {.from = source::setting, .key = normalised(asked.setting)},
    }};

    asked_for out;
    for (const named& s : sources) {
        if (s.key.empty()) {
            continue;
        }
        if (s.key == automatic) {
            // The picker's own "auto" is the default, not a request.
            if (s.from != source::setting) {
                out.from = s.from;
            }
            break;
        }
        if (contains(offered.available, s.key)) {
            out = {.key = s.key,
                   .from = s.from,
                   .by_name = s.from == source::command_line || s.from == source::environment};
            break;
        }
        notes.push_back("ignored '" + s.key + "' from " + std::string(describe(s.from)) +
                        ": this platform does not offer it");
    }
    return out;
}

}  // namespace

choice decide(const request& asked, const platform& offered, const record& r,
              std::string_view build, std::int64_t now) {
    choice out;
    out.guarded = !offered.automatic.empty();

    const asked_for wanted_by = read_sources(asked, offered, out.notes);
    out.from = wanted_by.from;

    std::vector<std::string> wanted;
    if (!wanted_by.key.empty()) {
        wanted.push_back(wanted_by.key);
    }
    for (const std::string& key : offered.automatic) {
        if (!contains(wanted, key)) {
            wanted.push_back(key);
        }
    }

    const std::vector<std::string> failed = active_failures(r, build, now, asked.setting);
    const std::string floor = offered.automatic.empty() ? std::string() : offered.automatic.back();
    for (const std::string& key : wanted) {
        if (contains(asked.skip, key)) {
            out.notes.push_back("skipped '" + key + "': it could not be used a moment ago");
            continue;
        }
        const bool exempt = (wanted_by.by_name && key == wanted_by.key) || key == floor;
        if (contains(failed, key) && !exempt) {
            out.notes.push_back("skipped '" + key +
                                "': it did not get as far as a frame on an earlier launch of "
                                "this build (name it with --graphics-api to try again)");
            continue;
        }
        out.chain.push_back(key);
    }
    return out;
}

record launch_started(const record& r, std::string_view backend) {
    record out = r;
    out.pending = std::string(backend);
    return out;
}

record launch_succeeded(const record& r, std::string_view backend) {
    record out = without_failure(r, backend);
    if (out.pending == backend) {
        out.pending.clear();
    }
    if (out.suspect == backend) {
        out.suspect.clear();
        out.strikes = 0;
    }
    return out;
}

record launch_ended(const record& r, std::string_view backend) {
    record out = r;
    if (out.pending == backend) {
        out.pending.clear();
    }
    return out;
}

record launch_failed(const record& r, std::string_view backend, std::string_view build,
                     std::int64_t now, std::string_view setting) {
    record out = with_failure(r, backend, build, now, setting);
    if (out.pending == backend) {
        out.pending.clear();
    }
    if (out.suspect == backend) {
        out.suspect.clear();
        out.strikes = 0;
    }
    return out;
}

}  // namespace cube::graphics
