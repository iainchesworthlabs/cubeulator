#include <cube/graphics/backend_choice.hpp>

#include <algorithm>
#include <cstddef>
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
    if (argument.size() > name.size() && argument.substr(0, name.size()) == name &&
        argument[name.size()] == '=') {
        return argument.substr(name.size() + 1);
    }
    return std::nullopt;
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
    r.failed.push_back({.backend = std::string(backend),
                        .build = std::string(build),
                        .when = now,
                        .setting = normalised(setting)});
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
    for (std::size_t i = 0; i < argv_tail.size(); ++i) {
        const std::string_view argument = argv_tail[i];
        if (const auto value = value_of(argument, option_graphics_api)) {
            parsed.graphics_api = normalised(*value);
        } else if (argument == option_graphics_api && i + 1 < argv_tail.size()) {
            parsed.graphics_api = normalised(argv_tail[++i]);
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

choice decide(const request& asked, const platform& offered, const record& r,
              std::string_view build, std::int64_t now) {
    choice out;
    out.guarded = !offered.automatic.empty();

    // Who is asking for what: the strongest source that names a backend this
    // platform can apply. "auto" settles it at once; an unknown key is set
    // aside with a note and the next source is read.
    struct candidate {
        source from;
        const std::string* value;
    };
    const std::string setting = normalised(asked.setting);
    const std::string command_line = asked.command_line ? normalised(*asked.command_line) : "";
    const std::string environment = asked.environment ? normalised(*asked.environment) : "";
    const std::string qt_environment =
        asked.qt_environment ? normalised(*asked.qt_environment) : "";
    const std::string none;
    const candidate candidates[] = {
        {source::command_line, asked.command_line ? &command_line : &none},
        {source::environment, asked.environment ? &environment : &none},
        {source::qt_environment, asked.qt_environment ? &qt_environment : &none},
        {source::setting, &setting},
    };

    std::string explicit_key;
    bool by_name = false;  // asked for on the command line or in CUBEULATOR_GRAPHICS_API
    for (const candidate& c : candidates) {
        if (c.value->empty()) {
            continue;
        }
        if (*c.value == automatic) {
            // The picker's own "auto" is the default, not a request.
            if (c.from != source::setting) {
                out.from = c.from;
            }
            break;
        }
        if (contains(offered.available, *c.value)) {
            explicit_key = *c.value;
            out.from = c.from;
            by_name = c.from == source::command_line || c.from == source::environment;
            break;
        }
        out.notes.push_back("ignored '" + *c.value + "' from " + std::string(describe(c.from)) +
                            ": this platform does not offer it");
    }

    std::vector<std::string> wanted;
    if (!explicit_key.empty()) {
        wanted.push_back(explicit_key);
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
        const bool exempt = (by_name && key == explicit_key) || key == floor;
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
