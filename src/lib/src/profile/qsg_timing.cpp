#include <cube/profile/qsg_timing.hpp>

#include <cstddef>
#include <optional>

namespace cube::profile {

namespace {

// The integer that follows `key` ("sync=" -> 12), if the key is present and a
// number follows. Qt prints milliseconds as whole numbers, with or without a
// trailing "ms".
std::optional<double> number_after(std::string_view text, std::string_view key) {
    std::size_t at = 0;
    while ((at = text.find(key, at)) != std::string_view::npos) {
        // The key must start a word: "render=" is not the tail of "prerender=".
        const bool word_start = at == 0 || std::string_view{" ,(["}.contains(text.at(at - 1));
        if (word_start) {
            double value = 0.0;
            bool any_digit = false;
            for (const char c : text.substr(at + key.size())) {
                if (c < '0' || c > '9') {
                    break;
                }
                value = (value * 10.0) + static_cast<double>(c - '0');
                any_digit = true;
            }
            if (any_digit) {
                return value;
            }
        }
        at += key.size();
    }
    return std::nullopt;
}

void add(qsg_line& line, std::string_view text, std::string_view key, const char* plot) {
    if (const auto value = number_after(text, key)) {
        line.samples.push_back({.plot = plot, .value = *value});
    }
}

}  // namespace

bool is_qsg_timing_category(std::string_view category) noexcept {
    return category.starts_with("qt.scenegraph.time.");
}

qsg_line parse_qsg_timing(const qsg_log_entry& entry) {
    const std::string_view category = entry.category;
    const std::string_view text = entry.text;
    qsg_line line;
    if (category == "qt.scenegraph.time.renderloop") {
        // "... syncAndRender: frame rendered in 0ms, sync=0, render=0, swap=0"
        // (threaded) and "Frame rendered with 'basic' renderloop in 4ms,
        // polish=0, sync=1, render=2, swap=1, frameDelta=16" (basic). The
        // other lines of the category ("start, elapsed since last call") are
        // frame boundaries the zones already give.
        if (!text.contains("rame rendered")) {
            return line;
        }
        add(line, text, "polish=", "qsg.polish_ms");
        add(line, text, "sync=", "qsg.sync_ms");
        add(line, text, "render=", "qsg.render_ms");
        add(line, text, "swap=", "qsg.swap_ms");
    } else if (category == "qt.scenegraph.time.renderer") {
        // "time in renderer: total=2ms, preprocess=2, updates=0, rendering=0"
        add(line, text, "total=", "qsg.renderer_ms");
        add(line, text, "preprocess=", "qsg.renderer_preprocess_ms");
        add(line, text, "updates=", "qsg.renderer_updates_ms");
        add(line, text, "rendering=", "qsg.renderer_rendering_ms");
    } else if (category == "qt.scenegraph.time.glyph") {
        // "distancefield: 38 glyphs prepared in 0ms, rendering=0, upload=0"
        add(line, text, "in ", "qsg.glyph_ms");
        add(line, text, "upload=", "qsg.glyph_upload_ms");
    } else if (category == "qt.scenegraph.time.texture" ||
               category == "qt.scenegraph.time.compilation") {
        // A texture uploaded, a shader or pipeline compiled: rare, and what
        // someone chasing a hitch wants named, so the line itself goes to the
        // timeline.
        line.as_message = true;
        if (category == "qt.scenegraph.time.texture") {
            add(line, text, "total=", "qsg.texture_ms");
            add(line, text, "upload=", "qsg.texture_upload_ms");
        }
    }
    return line;
}

}  // namespace cube::profile
