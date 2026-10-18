// Direct3D 11 GPU context for cube::profiling::GpuContext - the backend for
// Windows, compiled by src/lib/CMakeLists.txt in place of
// tracy_gpu_d3d11_stub.cpp wherever the platform is WIN32. The OpenGL
// backend is src/profile/tracy_gpu.cpp; nothing here tests the OS because
// CMake already chose this file.
//
// The app extracts the device and its immediate context from its window's
// QSGRendererInterface and passes them in as void*; this file stays free of
// Qt. The NOMINMAX compile definition src/lib/CMakeLists.txt gives this
// file keeps <windows.h>'s min/max macros out of the standard library.
//
// The context is created and driven on the scene graph's render thread,
// where the immediate context's command stream is the one being drawn:
// begin_zone() inserts a begin timestamp before the frame's draw calls,
// end_zone() the end timestamp after them, and collect() resolves what the
// GPU has completed. It is intentionally never destroyed: it AddRefs the
// device, and destruction would have to run on the render thread after the
// window is gone.

#include <cube/profiling.hpp>

#include <windows.h>

#include <d3d11.h>

#include <tracy/TracyD3D11.hpp>

#include <cstdint>
#include <optional>

namespace {

class D3D11GpuContext final : public cube::profiling::GpuContext {
public:
    explicit D3D11GpuContext(ID3D11Device* device, ID3D11DeviceContext* device_context) {
        if (device == nullptr) {
            return;
        }
        if (device_context == nullptr) {
            device->GetImmediateContext(&device_context);
        }
        if (device_context == nullptr) {
            return;
        }
        ctx_ = TracyD3D11Context(device, device_context);
        if (ctx_ != nullptr) {
            TracyD3D11ContextName(ctx_, "render", 6);
        }
    }

    bool valid() const noexcept override { return ctx_ != nullptr; }

    void begin_zone(const char* name) noexcept override {
        if (ctx_ == nullptr) {
            return;
        }
        location_ = tracy::SourceLocationData{name, __func__, __FILE__, 0, 0};
        zone_.emplace(ctx_, &location_, true);
    }

    void end_zone() noexcept override { zone_.reset(); }

    void collect() noexcept override {
        if (ctx_ != nullptr) {
            TracyD3D11Collect(ctx_);
        }
    }

private:
    TracyD3D11Ctx ctx_ = nullptr;
    tracy::SourceLocationData location_{};
    std::optional<tracy::D3D11ZoneScope> zone_;
};

}  // namespace

namespace cube::profiling {

GpuContext* GpuContext::create_d3d11(void* device, void* device_context) noexcept {
    auto* d3d_device = static_cast<ID3D11Device*>(device);
    auto* d3d_context = static_cast<ID3D11DeviceContext*>(device_context);
    if (d3d_device == nullptr) {
        return nullptr;
    }
    auto* context = new D3D11GpuContext(d3d_device, d3d_context);
    if (!context->valid()) {
        delete context;
        return nullptr;
    }
    return context;
}

}  // namespace cube::profiling
