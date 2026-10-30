// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string_view>

namespace cube::platform::graphics_probe {

// Whether the graphics backend named by `backend` (a key of
// platform_info::GraphicsBackend: "vulkan", "d3d12", "d3d11", ...) can start on
// this machine, found out without Qt and without a window.
//
// Qt does not fail cleanly when a backend's own library or driver is missing:
// asked for Vulkan with no usable loader or driver, Qt 6.12 logs "Failed to
// create platform Vulkan instance" and then dereferences the instance it does
// not have - an access violation before the first frame. So the choice of API
// (desktop/graphics_backend.hpp) asks first, and goes down its chain past a
// backend that says no. An answer of `true` is not a promise (a driver can
// still fail later; the launch guard is for that), but `false` is always right.
//
// There is exactly one definition per platform, living in
// platform/<os>/graphics_probe_<os>.cpp, or platform/posix/graphics_probe_none.cpp
// where nothing can be asked cheaply (every backend is reported usable). CMake
// compiles only the file the platform's fragment names.
[[nodiscard]] bool usable(std::string_view backend);

}  // namespace cube::platform::graphics_probe
