// The no-Vulkan backend of cube::profiling::GpuContext, for every platform
// whose platform.cmake does not set CUBEULATOR_PLATFORM_PROFILING_VULKAN (see
// tracy_gpu_vulkan.cpp). create_vulkan cannot succeed where the application
// is never given a Vulkan device, so it returns null.

#include <cube/profiling.hpp>

namespace cube::profiling {

GpuContext* GpuContext::create_vulkan(const vulkan_gpu_handles& /*handles*/) noexcept {
    return nullptr;
}

}  // namespace cube::profiling
