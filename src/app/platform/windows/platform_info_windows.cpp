// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "interfaces/platform_info.hpp"

namespace cube::platform::platform_info {

PlatformInfo current() {
    return PlatformInfo{
        .is_mobile = false,
        .graphics_backends =
            {
                {.key = "d3d11", .label = "D3D11"},
                {.key = "d3d12", .label = "D3D12"},
                {.key = "vulkan", .label = "Vulkan"},
                {.key = "opengl", .label = "GL"},
            },
        // Direct3D 11 is what Qt picks here and what this application has
        // always shipped on; docs/graphics-api.md has the numbers for and
        // against Vulkan. The floor is Direct3D 11 on WARP, the software
        // rasteriser every Windows has, so a machine with no usable GPU
        // driver (a VM, a remote session) still opens a window.
        .automatic_graphics_backends = {"d3d11", "software"},
        .software_graphics_api = "d3d11",
        .japanese_faces = {"Yu Gothic UI", "Meiryo UI", "MS UI Gothic"},
        .chinese_faces = {"Microsoft YaHei UI", "Microsoft YaHei"},
    };
}

}  // namespace cube::platform::platform_info
