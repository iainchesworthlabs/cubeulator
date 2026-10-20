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
        // Vulkan first: the GPU spends less on a frame and the scan screen draws
        // faster; it starts about 150 ms later and holds about 100 MB more
        // graphics memory (docs/graphics-api.md has the numbers). A machine that cannot use it (no
        // loader, no driver) is sent on by graphics_probe, and one that crashes it by the launch
        // guard. Direct3D 12 is deliberately not in the chain: it takes seconds
        // to reach a first 3D frame where the others take a second. The floor is
        // Direct3D 11 on WARP, the software rasteriser every Windows has, so a
        // machine with no usable GPU driver (a VM, a remote session) still
        // opens a window.
        .automatic_graphics_backends = {"vulkan", "d3d11", "software"},
        .software_graphics_api = "d3d11",
        .japanese_faces = {"Yu Gothic UI", "Meiryo UI", "MS UI Gothic"},
        .chinese_faces = {"Microsoft YaHei UI", "Microsoft YaHei"},
    };
}

}  // namespace cube::platform::platform_info
