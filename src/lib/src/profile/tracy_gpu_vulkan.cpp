// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// Vulkan GPU context for cube::profiling::GpuContext: Tracy's per-queue VkCtx
// behind the same interface as the OpenGL and Direct3D ones. Built into cube::lib
// when Tracy is on and the platform's fragment says it has Vulkan
// (CUBEULATOR_PLATFORM_PROFILING_VULKAN, see src/lib/CMakeLists.txt); the
// platforms without it compile tracy_gpu_vulkan_none.cpp instead. Nothing in
// this file tests the OS: Vulkan's header and its calling convention are the
// same on every platform that has it.
//
// The app hands over the instance, the physical device, the device and the
// queue the scene graph renders with (QSGRendererInterface), and the loader's
// two entry points. Tracy resolves every Vulkan function through those
// (TRACY_VK_USE_SYMBOL_TABLE), so this library links no Vulkan loader and does
// not care whether the application's Qt found one.
//
// A Vulkan timestamp is a command recorded into a command buffer, so
// begin_zone()/end_zone() write into the buffer they are handed - the frame's,
// which the scene graph is recording - and collect() records the query-pool
// reset for the results it has just read into the buffer it is given. It must
// therefore be called outside a render pass. Tracy's context construction runs
// two tiny submissions of a command buffer of its own on the queue; this file
// makes one for it and frees it afterwards.
//
// The context is intentionally never destroyed (it owns a query pool of the
// device, which outlives nothing it could be torn down before).

#include <cube/profiling.hpp>

#include <cstdint>
#include <optional>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#define TRACY_VK_USE_SYMBOL_TABLE
#include <tracy/TracyVulkan.hpp>

namespace {

// A Vulkan entry point arrives as the generic function pointer (or void*) the
// loader hands out; turning it into its own type is the whole job here.
template <typename To, typename From>
To as_function(From from) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<To>(from);
}

class VulkanGpuContext final : public cube::profiling::GpuContext {
public:
    explicit VulkanGpuContext(const cube::profiling::vulkan_gpu_handles& handles) {
        const auto get_instance_proc =
            as_function<PFN_vkGetInstanceProcAddr>(handles.get_instance_proc_addr);
        const auto get_device_proc =
            as_function<PFN_vkGetDeviceProcAddr>(handles.get_device_proc_addr);
        auto* instance = static_cast<VkInstance>(handles.instance);
        auto* physical_device = static_cast<VkPhysicalDevice>(handles.physical_device);
        auto* device = static_cast<VkDevice>(handles.device);
        auto* queue = static_cast<VkQueue>(handles.queue);

        const auto create_pool =
            as_function<PFN_vkCreateCommandPool>(get_device_proc(device, "vkCreateCommandPool"));
        const auto destroy_pool =
            as_function<PFN_vkDestroyCommandPool>(get_device_proc(device, "vkDestroyCommandPool"));
        const auto allocate = as_function<PFN_vkAllocateCommandBuffers>(
            get_device_proc(device, "vkAllocateCommandBuffers"));
        if (create_pool == nullptr || destroy_pool == nullptr || allocate == nullptr) {
            return;
        }

        // The command buffer Tracy records its initial queries into.
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool_info.queueFamilyIndex = handles.queue_family;
        VkCommandPool pool = VK_NULL_HANDLE;
        if (create_pool(device, &pool_info, nullptr, &pool) != VK_SUCCESS) {
            return;
        }
        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        VkCommandBuffer scratch = VK_NULL_HANDLE;
        if (allocate(device, &allocate_info, &scratch) == VK_SUCCESS) {
            ctx_ = TracyVkContext(instance, physical_device, device, queue, scratch,
                                  get_instance_proc, get_device_proc);
            if (ctx_ != nullptr) {
                TracyVkContextName(ctx_, "render", 6);
            }
        }
        destroy_pool(device, pool, nullptr);  // frees the buffer with it
    }

    [[nodiscard]] bool valid() const noexcept override { return ctx_ != nullptr; }

    void begin_zone(const char* name, void* command_stream) noexcept override {
        if (ctx_ == nullptr || command_stream == nullptr) {
            return;
        }
        location_ = tracy::SourceLocationData{
            .name = name,
            .function = __func__,
            .file = __FILE__,
            .line = 0,
            .color = 0,
        };
        zone_.emplace(ctx_, &location_, static_cast<VkCommandBuffer>(command_stream), true);
    }

    void end_zone() noexcept override { zone_.reset(); }

    void collect(void* command_stream) noexcept override {
        if (ctx_ != nullptr && command_stream != nullptr) {
            TracyVkCollect(ctx_, static_cast<VkCommandBuffer>(command_stream));
        }
    }

private:
    TracyVkCtx ctx_ = nullptr;
    tracy::SourceLocationData location_{};
    std::optional<tracy::VkCtxScope> zone_;
};

}  // namespace

namespace cube::profiling {

GpuContext* GpuContext::create_vulkan(const vulkan_gpu_handles& handles) noexcept {
    if (!active() || handles.instance == nullptr || handles.physical_device == nullptr ||
        handles.device == nullptr || handles.queue == nullptr ||
        handles.get_instance_proc_addr == nullptr || handles.get_device_proc_addr == nullptr) {
        return nullptr;
    }
    // Never destroyed on purpose (see the top of the file); a failed one is.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory,bugprone-unhandled-exception-at-new)
    auto* context = new VulkanGpuContext(handles);
    if (!context->valid()) {
        delete context;  // NOLINT(cppcoreguidelines-owning-memory)
        return nullptr;
    }
    return context;
}

}  // namespace cube::profiling
