// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Reads the lines Qt Quick's scene graph logs about its own timing
// (qt.scenegraph.time.* logging categories) into numbers a profiler can plot.
//
// Qt does not expose the phases a frame spends inside the scene graph - the
// synchronisation, the renderer's preprocess (where Quick 3D renders its
// offscreen passes and first-use shaders and pipelines are built), glyph
// preparation, texture uploads, shader compilation, the swap - other than by
// logging them. The application's Qt message handler (src/app/qt/
// qsg_log_bridge.cpp) hands each such line to parse_qsg_timing() and forwards
// the result to the profiler; keeping the reading here, free of Qt, is what
// lets a test pin it against lines copied from a real run.
//
// The line formats are Qt's, not a contract: a line the parser does not
// recognise yields no samples and is forwarded as text only if its category
// asks for it, so a Qt that words them differently loses numbers, never
// events.

#include <string_view>
#include <vector>

namespace cube::profile {

struct qsg_sample {
    // A plot name, always a string literal (Tracy keeps the pointer).
    const char* plot = nullptr;
    double value = 0.0;
};

struct qsg_line {
    std::vector<qsg_sample> samples;
    // The line is rare and worth reading as text on the timeline (a texture
    // upload, a shader compilation), as opposed to a per-frame number.
    bool as_message = false;
};

// One line of the scene graph's log: the logging category it came under and
// its message.
struct qsg_log_entry {
    std::string_view category;
    std::string_view text;
};

// True for the categories this reads: the qt.scenegraph.time.* family.
[[nodiscard]] bool is_qsg_timing_category(std::string_view category) noexcept;

[[nodiscard]] qsg_line parse_qsg_timing(const qsg_log_entry& entry);

}  // namespace cube::profile
