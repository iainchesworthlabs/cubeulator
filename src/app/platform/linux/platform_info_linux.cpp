#include "interfaces/platform_info.hpp"

namespace cube::platform::platform_info {

PlatformInfo current() {
    return PlatformInfo{
        .is_mobile = false,
        .graphics_backends =
            {
                {.key = "vulkan", .label = "Vulkan"},
                {.key = "opengl", .label = "GL"},
            },
        // Vulkan first, as on Windows; OpenGL (Mesa's software rasteriser if
        // there is no GPU) is the floor. Not run on a Linux machine yet.
        .automatic_graphics_backends = {"vulkan", "opengl"},
        .japanese_faces = {"Noto Sans CJK JP", "Noto Sans JP", "Source Han Sans JP"},
        .chinese_faces =
            {
                "Noto Sans CJK SC",
                "Noto Sans SC",
                "Source Han Sans SC",
                "WenQuanYi Micro Hei",
            },
    };
}

}  // namespace cube::platform::platform_info
