// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// The no-Direct3D backend of cube::profiling::GpuContext, for every platform
// whose platform.cmake does not name windows/tracy_gpu_d3d11_windows.cpp in
// CUBEULATOR_PLATFORM_PROFILING_GPU_SOURCES. create_d3d11 cannot succeed where
// the app can never hand it a device, so it returns null; the app logs
// render.gpu_unavailable and keeps the CPU zones (see
// src/app/qt/render_profiler.cpp).

#include <cube/profiling.hpp>

namespace cube::profiling {

GpuContext* GpuContext::create_d3d11(void* /*device*/, void* /*device_context*/) noexcept {
    return nullptr;
}

}  // namespace cube::profiling
