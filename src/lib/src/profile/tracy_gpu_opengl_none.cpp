// The no-OpenGL backend of cube::profiling::GpuContext, for every platform
// whose platform.cmake does not set CUBEULATOR_PLATFORM_PROFILING_OPENGL (see
// tracy_gpu.cpp, and why Apple is one). create_opengl cannot succeed where the
// application is never given a GL context to time, so it returns null.

#include <cube/profiling.hpp>

namespace cube::profiling {

GpuContext* GpuContext::create_opengl(const gl_gpu_functions& /*functions*/) noexcept {
    return nullptr;
}

}  // namespace cube::profiling
