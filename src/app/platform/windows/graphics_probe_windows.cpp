// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// Windows: Vulkan through vulkan-1.dll (installed by the GPU's driver), and
// Direct3D 12 and 11 by creating a device on the default adapter and throwing
// it away. Software rendering (Direct3D 11 on WARP) is always there.
#include "interfaces/graphics_probe.hpp"
#include "platform/detail/vulkan_probe.hpp"

#include <windows.h>
// d3d11.h and d3d12.h need windows.h first.
#include <d3d11.h>
#include <d3d12.h>

namespace cube::platform::graphics_probe {

namespace {

// Loads `dll` from the system directory only: the application's own directory is
// not where a graphics driver's library lives, and a stray file there should
// not decide which API the window uses.
HMODULE load_system_library(const wchar_t* dll) noexcept {
    return ::LoadLibraryExW(dll, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
}

bool vulkan_usable() noexcept {
    const HMODULE loader = load_system_library(L"vulkan-1.dll");
    if (loader == nullptr) {
        return false;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto get = reinterpret_cast<detail::VulkanGetInstanceProcAddr>(
        ::GetProcAddress(loader, "vkGetInstanceProcAddr"));
    return detail::vulkan_has_a_device(get);
}

bool d3d12_usable() noexcept {
    const HMODULE library = load_system_library(L"d3d12.dll");
    if (library == nullptr) {
        return false;
    }
    using Create = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto create = reinterpret_cast<Create>(::GetProcAddress(library, "D3D12CreateDevice"));
    // With no output pointer the call only reports whether a device could be made.
    return create != nullptr &&
           SUCCEEDED(create(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
}

bool d3d11_usable() noexcept {
    const HMODULE library = load_system_library(L"d3d11.dll");
    if (library == nullptr) {
        return false;
    }
    using Create =
        HRESULT(WINAPI*)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
                         UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto create = reinterpret_cast<Create>(::GetProcAddress(library, "D3D11CreateDevice"));
    return create != nullptr &&
           SUCCEEDED(create(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                            D3D11_SDK_VERSION, nullptr, nullptr, nullptr));
}

}  // namespace

bool usable(std::string_view backend) {
    if (backend == "vulkan") {
        return vulkan_usable();
    }
    if (backend == "d3d12") {
        return d3d12_usable();
    }
    if (backend == "d3d11") {
        return d3d11_usable();
    }
    return true;
}

}  // namespace cube::platform::graphics_probe
