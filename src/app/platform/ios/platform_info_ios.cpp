// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "interfaces/platform_info.hpp"

namespace cube::platform::platform_info {

PlatformInfo current() {
    // iOS effectively has just the one real choice.
    return PlatformInfo{
        .is_mobile = true,
        .camera_switch_on_scan_screen = true,
        .graphics_backends = {{.key = "metal", .label = "Metal"}},
        .automatic_graphics_backends = {},
        .software_graphics_api = {},
        .japanese_faces = {"Hiragino Sans", "Hiragino Kaku Gothic ProN"},
        .chinese_faces = {"PingFang SC", "Heiti SC"},
    };
}

}  // namespace cube::platform::platform_info
