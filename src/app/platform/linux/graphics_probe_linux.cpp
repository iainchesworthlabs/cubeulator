// Linux: Vulkan is asked through libvulkan.so.1; every other backend (OpenGL
// finds Mesa's software rasteriser when there is no GPU) is reported usable.
#include "interfaces/graphics_probe.hpp"
#include "platform/detail/vulkan_probe.hpp"

#include <dlfcn.h>

namespace cube::platform::graphics_probe {

bool usable(std::string_view backend) {
    if (backend != "vulkan") {
        return true;
    }
    void* loader = ::dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (loader == nullptr) {
        return false;
    }
    // dlsym returns an object pointer; the loader's entry point is a function.
    // The library stays loaded: Qt opens the same one a moment later.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto get = reinterpret_cast<detail::VulkanGetInstanceProcAddr>(
        ::dlsym(loader, "vkGetInstanceProcAddr"));
    return detail::vulkan_has_a_device(get);
}

}  // namespace cube::platform::graphics_probe
