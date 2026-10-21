// The no-Vulkan half of the render profiler, for every platform whose fragment
// does not set CUBEULATOR_PLATFORM_PROFILING_VULKAN (or whose build has no
// Tracy): see render_profiler_vulkan.cpp. A window on such a platform is never
// rendering with Vulkan, so there is nothing to find.

#include "render_profiler.hpp"

namespace cube::app {

::cube::profiling::GpuContext* createVulkanGpuContext(QQuickWindow& /*window*/) {
    return nullptr;
}

void* vulkanCommandBuffer(QQuickWindow& /*window*/) {
    return nullptr;
}

}  // namespace cube::app
