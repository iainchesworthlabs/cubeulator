// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace cube::app {

// Carries Qt Quick's own scene-graph timing into the profiler.
//
// The scene graph logs, per frame and per event, what it spent time on - the
// renderer's preprocess (Quick 3D's offscreen passes and first-use shader and
// pipeline builds), glyph preparation, texture uploads, shader compilation,
// the sync/render/swap split - under the qt.scenegraph.time.* logging
// categories, and exposes none of it any other way. install() turns those
// categories on and installs a Qt message handler that hands each line to
// cube::profile::parse_qsg_timing: the numbers become Tracy plots
// (qsg.sync_ms, qsg.swap_ms, qsg.renderer_preprocess_ms, ...) and the rare
// events (a texture uploaded, a shader compiled) become messages.
//
// Call it once, early, and only when the profiler is running: the categories
// cost Qt a clock read per phase and a formatted line per frame. The lines are
// kept off the console unless QT_LOGGING_RULES names them, so a profiled
// Debug build does not start printing a screenful a second; every other
// message goes to whatever handler was installed before.
void installQsgLogBridge();

}  // namespace cube::app
