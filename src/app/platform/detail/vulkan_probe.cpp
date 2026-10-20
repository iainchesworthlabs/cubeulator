#include "platform/detail/vulkan_probe.hpp"

#include <cstdint>

namespace cube::platform::detail {

namespace {

// The few Vulkan declarations the probe needs. They are the C ABI of Vulkan 1.0
// (VkApplicationInfo, VkInstanceCreateInfo), which never changes.
struct ApplicationInfo {
    std::int32_t type;  // VK_STRUCTURE_TYPE_APPLICATION_INFO
    const void* next;
    const char* application_name;
    std::uint32_t application_version;
    const char* engine_name;
    std::uint32_t engine_version;
    std::uint32_t api_version;
};

struct InstanceCreateInfo {
    std::int32_t type;  // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    const void* next;
    std::uint32_t flags;
    const ApplicationInfo* application_info;
    std::uint32_t enabled_layer_count;
    const char* const* enabled_layer_names;
    std::uint32_t enabled_extension_count;
    const char* const* enabled_extension_names;
};

constexpr std::int32_t kApplicationInfo = 0;
constexpr std::int32_t kInstanceCreateInfo = 1;
constexpr std::uint32_t kApiVersion10 = 1U << 22U;  // VK_MAKE_API_VERSION(0, 1, 0, 0)

using CreateInstance = std::int32_t (*)(const InstanceCreateInfo*, const void*, void**);
using EnumeratePhysicalDevices = std::int32_t (*)(void*, std::uint32_t*, void*);
using DestroyInstance = void (*)(void*, const void*);

template <typename Function>
Function resolve(VulkanGetInstanceProcAddr get, void* instance, const char* name) noexcept {
    // A loader returns every entry point as void(*)(); the cast restores its type.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return reinterpret_cast<Function>(get(instance, name));
}

}  // namespace

bool vulkan_has_a_device(VulkanGetInstanceProcAddr get) noexcept {
    if (get == nullptr) {
        return false;
    }
    const auto create = resolve<CreateInstance>(get, nullptr, "vkCreateInstance");
    if (create == nullptr) {
        return false;
    }

    const ApplicationInfo application{
        .type = kApplicationInfo,
        .next = nullptr,
        .application_name = "Cubeulator graphics probe",
        .application_version = 0,
        .engine_name = nullptr,
        .engine_version = 0,
        .api_version = kApiVersion10,
    };
    const InstanceCreateInfo info{
        .type = kInstanceCreateInfo,
        .next = nullptr,
        .flags = 0,
        .application_info = &application,
        .enabled_layer_count = 0,
        .enabled_layer_names = nullptr,
        .enabled_extension_count = 0,
        .enabled_extension_names = nullptr,
    };
    void* instance = nullptr;
    if (create(&info, nullptr, &instance) != 0 || instance == nullptr) {
        return false;
    }

    bool found = false;
    if (const auto enumerate =
            resolve<EnumeratePhysicalDevices>(get, instance, "vkEnumeratePhysicalDevices")) {
        std::uint32_t count = 0;
        found = enumerate(instance, &count, nullptr) == 0 && count > 0;
    }
    if (const auto destroy = resolve<DestroyInstance>(get, instance, "vkDestroyInstance")) {
        destroy(instance, nullptr);
    }
    return found;
}

}  // namespace cube::platform::detail
