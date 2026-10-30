// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Can a Vulkan instance be created, and does it see a GPU? Asked through the
// loader's own entry point with hand-declared types, so this needs neither the
// Vulkan headers nor Qt, and the same file serves every platform that has a
// loader: the platform opens the library (vulkan-1.dll, libvulkan.so.1) and
// hands over its `vkGetInstanceProcAddr`.

namespace cube::platform::detail {

// `vkGetInstanceProcAddr` of a loaded Vulkan loader, as a plain function
// pointer: vkGetInstanceProcAddr(instance, name) -> void(*)().
using VulkanVoidFunction = void (*)();
using VulkanGetInstanceProcAddr = VulkanVoidFunction (*)(void* instance, const char* name);

// True when vkCreateInstance succeeds and at least one physical device is
// enumerated; the instance is destroyed again. False for a null pointer.
[[nodiscard]] bool vulkan_has_a_device(VulkanGetInstanceProcAddr get_instance_proc_addr) noexcept;

}  // namespace cube::platform::detail
