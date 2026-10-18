// The no-Direct3D-11 backend of cube::profiling::GpuContext: compiled by
// src/lib/CMakeLists.txt in place of tracy_gpu_d3d11.cpp wherever the
// platform is not WIN32. create_d3d11 cannot succeed where the app can
// never hand it a device, so it returns null; the app logs
// render.gpu_unavailable and keeps the CPU zone (see
// src/app/qt/render_profiler.cpp).

#include <cube/profiling.hpp>

namespace cube::profiling {

GpuContext* GpuContext::create_d3d11(void*, void*) noexcept { return nullptr; }

}  // namespace cube::profiling
