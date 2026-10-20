// The Vulkan objects Tracy's GPU context needs, found the way Qt hands them
// out: QSGRendererInterface resources of the window being rendered, and the
// loader's two entry points through its QVulkanInstance. Built only on the
// platforms whose fragment sets CUBEULATOR_PLATFORM_PROFILING_VULKAN (the
// Vulkan headers come from the vulkan-headers port); the others compile
// render_profiler_vulkan_none.cpp.
//
// Everything here runs on the scene graph's render thread: the resources are
// valid only there and only while the window is rendering.

#include "render_profiler.hpp"

#include <cube/profiling.hpp>

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QVulkanInstance>

#include <cstdint>

namespace cube::app {

::cube::profiling::GpuContext* createVulkanGpuContext(QQuickWindow& window) {
    QSGRendererInterface* renderer = window.rendererInterface();
    if (renderer == nullptr) {
        return nullptr;
    }
    auto* instance = static_cast<QVulkanInstance*>(
        renderer->getResource(&window, QSGRendererInterface::VulkanInstanceResource));
    // Vulkan's handles are given by address: a pointer to the VkDevice, not
    // the VkDevice.
    auto* physical_device = static_cast<VkPhysicalDevice*>(
        renderer->getResource(&window, QSGRendererInterface::PhysicalDeviceResource));
    auto* device = static_cast<VkDevice*>(
        renderer->getResource(&window, QSGRendererInterface::DeviceResource));
    auto* queue = static_cast<VkQueue*>(
        renderer->getResource(&window, QSGRendererInterface::CommandQueueResource));
    auto* family = static_cast<std::uint32_t*>(
        renderer->getResource(&window, QSGRendererInterface::GraphicsQueueFamilyIndexResource));
    if (instance == nullptr || physical_device == nullptr || device == nullptr ||
        queue == nullptr || family == nullptr) {
        return nullptr;
    }

    // The loader hands out its own two entry points by name; Tracy takes
    // everything else through them.
    const QFunctionPointer get_instance_proc =
        instance->getInstanceProcAddr("vkGetInstanceProcAddr");
    const QFunctionPointer get_device_proc = instance->getInstanceProcAddr("vkGetDeviceProcAddr");

    ::cube::profiling::vulkan_gpu_handles handles;
    handles.instance = instance->vkInstance();
    handles.physical_device = *physical_device;
    handles.device = *device;
    handles.queue = *queue;
    handles.queue_family = *family;
    handles.get_instance_proc_addr = reinterpret_cast<void*>(get_instance_proc);
    handles.get_device_proc_addr = reinterpret_cast<void*>(get_device_proc);
    return ::cube::profiling::GpuContext::create_vulkan(handles);
}

void* vulkanCommandBuffer(QQuickWindow& window) {
    QSGRendererInterface* renderer = window.rendererInterface();
    if (renderer == nullptr) {
        return nullptr;
    }
    auto* buffer = static_cast<VkCommandBuffer*>(
        renderer->getResource(&window, QSGRendererInterface::CommandListResource));
    return buffer == nullptr ? nullptr : *buffer;
}

}  // namespace cube::app
