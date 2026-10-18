// GPU (render-thread) profiling: the Tracy implementations behind
// cube::profiling::GpuContext, built into cube::lib only when
// CUBEULATOR_ENABLE_TRACY is on (see src/lib/CMakeLists.txt). The sibling
// variants' cube/profiling.hpp declare the same classes as no-ops, so the
// app's render thread compiles unchanged whatever the build.
//
// Everything the Tracy GPU bindings need is header-heavy - <d3d11.h> on
// Windows and a set of renamed OpenGL entry points - so it stays in this one
// translation unit. The app extracts the native handles from its window's
// QSGRendererInterface/QOpenGLContext and passes them in as void* and
// function pointers; this file stays free of Qt.
//
// A context is created and driven on the scene graph's render thread, where
// the graphics context is current and the command stream is the one being
// drawn. begin_zone() inserts a begin timestamp into the stream before the
// frame's draw calls, end_zone() inserts the end timestamp after them, and
// collect() resolves what the GPU has completed.
//
// Contexts are intentionally never destroyed: a window outlives everything
// they hold a reference to (the D3D11 context AddRefs its device, and the
// OpenGL context is Tracy's own per-thread object), and destruction would
// have to run on the render thread after the window is gone.

#include <cube/profiling.hpp>

#include <cstdint>
#include <optional>

// ---------------------------------------------------------------------------
// OpenGL (TracyOpenGL.hpp)
// ---------------------------------------------------------------------------
//
// TracyOpenGL.hpp calls the GL entry points as plain function names and
// picks GL1/GL3 token spellings with #ifdefs, so this file defines the
// tokens and types a GL header would (plain C integers: GLint/GLuint are
// 32-bit and GLint64/GLuint64 are 64-bit on every platform the app builds
// on) and renames the function names onto the pointers the app resolved
// from its QOpenGLContext. A core-profile context is assumed -
// GL_VERSION_3_0 makes the header ask glGetStringi for extensions, because
// the GL1 glGetString(GL_EXTENSIONS) query is not legal there.
//
// The include sits at file scope, not inside an anonymous namespace: the
// header reopens `namespace tracy`, and a namespace opened inside an
// anonymous namespace would shadow Tracy's own types with empty ones.

#define GL_TIMESTAMP 0x8E28
#define GL_QUERY_COUNTER_BITS 0x8864
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_NO_ERROR 0x0
#define GL_EXTENSIONS 0x1F03
#define GL_FALSE 0x0
#define GL_TRUE 0x1
#define GL_VERSION_3_0 1

namespace {

using gl_enum = std::uint32_t;
using gl_uint = std::uint32_t;
using gl_int = std::int32_t;
using gl_sizei = std::int32_t;
using gl_int64 = std::int64_t;
using gl_uint64 = std::uint64_t;
using gl_ubyte = std::uint8_t;

void (*gl_gen_queries)(gl_sizei, gl_uint*) = nullptr;
void (*gl_get_queryiv)(gl_enum, gl_enum, gl_int*) = nullptr;
void (*gl_get_query_objectiv)(gl_uint, gl_enum, gl_int*) = nullptr;
void (*gl_get_query_objectui64v)(gl_uint, gl_enum, gl_uint64*) = nullptr;
void (*gl_get_integer64v)(gl_enum, gl_int64*) = nullptr;
void (*gl_query_counter)(gl_uint, gl_enum) = nullptr;
gl_enum (*gl_get_error)() = nullptr;
void (*gl_get_integerv)(gl_enum, gl_int*) = nullptr;
const gl_ubyte* (*gl_get_string)(gl_enum) = nullptr;
const gl_ubyte* (*gl_get_stringi)(gl_enum, gl_uint) = nullptr;

// The header spells its scalar locals as GLint/GLenum/...; without a GL
// header those are these.
using GLint = gl_int;
using GLenum = gl_enum;
using GLuint = gl_uint;
using GLsizei = gl_sizei;
using GLint64 = gl_int64;
using GLuint64 = gl_uint64;
using GLubyte = gl_ubyte;

}  // namespace

#define glGenQueries gl_gen_queries
#define glGetQueryiv gl_get_queryiv
#define glGetQueryObjectiv gl_get_query_objectiv
#define glGetQueryObjectui64v gl_get_query_objectui64v
#define glGetInteger64v gl_get_integer64v
#define glQueryCounter gl_query_counter
#define glGetError gl_get_error
#define glGetIntegerv gl_get_integerv
#define glGetString gl_get_string
#define glGetStringi gl_get_stringi

#include <tracy/TracyOpenGL.hpp>

#undef glGenQueries
#undef glGetQueryiv
#undef glGetQueryObjectiv
#undef glGetQueryObjectui64v
#undef glGetInteger64v
#undef glQueryCounter
#undef glGetError
#undef glGetIntegerv
#undef glGetString
#undef glGetStringi

namespace {

// The GL context is Tracy's own thread-local object (tracy::GetGpuCtx());
// this class is just the handle the app drives it through.
class OpenglGpuContext final : public cube::profiling::GpuContext {
public:
    OpenglGpuContext() {
        // Constructs the per-thread GL context. The entry-point pointers
        // above and a current GL context must already be in place.
        TracyGpuContext;
        valid_ = true;
    }

    bool valid() const noexcept override { return valid_; }

    void begin_zone(const char* name) noexcept override {
        if (!valid_) {
            return;
        }
        location_ = tracy::SourceLocationData{name, __func__, __FILE__, 0, 0};
        zone_.emplace(&location_, true);
    }

    void end_zone() noexcept override { zone_.reset(); }

    void collect() noexcept override {
        if (valid_) {
            TracyGpuCollect;
        }
    }

private:
    bool valid_ = false;
    tracy::SourceLocationData location_{};
    std::optional<tracy::GpuCtxScope> zone_;
};

}  // namespace

namespace cube::profiling {

GpuContext* GpuContext::create_opengl(const gl_gpu_functions& functions) noexcept {
    if (functions.gen_queries == nullptr || functions.get_queryiv == nullptr ||
        functions.get_query_objectiv == nullptr ||
        functions.get_query_objectui64v == nullptr ||
        functions.get_integer64v == nullptr || functions.query_counter == nullptr ||
        functions.get_error == nullptr || functions.get_integerv == nullptr ||
        functions.get_string == nullptr || functions.get_stringi == nullptr) {
        return nullptr;
    }

    gl_gen_queries = reinterpret_cast<decltype(gl_gen_queries)>(functions.gen_queries);
    gl_get_queryiv = reinterpret_cast<decltype(gl_get_queryiv)>(functions.get_queryiv);
    gl_get_query_objectiv =
        reinterpret_cast<decltype(gl_get_query_objectiv)>(functions.get_query_objectiv);
    gl_get_query_objectui64v =
        reinterpret_cast<decltype(gl_get_query_objectui64v)>(functions.get_query_objectui64v);
    gl_get_integer64v =
        reinterpret_cast<decltype(gl_get_integer64v)>(functions.get_integer64v);
    gl_query_counter = reinterpret_cast<decltype(gl_query_counter)>(functions.query_counter);
    gl_get_error = reinterpret_cast<decltype(gl_get_error)>(functions.get_error);
    gl_get_integerv = reinterpret_cast<decltype(gl_get_integerv)>(functions.get_integerv);
    gl_get_string = reinterpret_cast<decltype(gl_get_string)>(functions.get_string);
    gl_get_stringi = reinterpret_cast<decltype(gl_get_stringi)>(functions.get_stringi);

    // One GL context per thread; a second call (another window) reuses it.
    // The pointers above describe the same thread's current context either
    // way.
    static thread_local OpenglGpuContext context;
    return &context;
}

}  // namespace cube::profiling

// ---------------------------------------------------------------------------
// Direct3D 11 (TracyD3D11.hpp) - Windows only
// ---------------------------------------------------------------------------
#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d11.h>

#include <tracy/TracyD3D11.hpp>

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

#else  // !_WIN32

namespace cube::profiling {

GpuContext* GpuContext::create_d3d11(void*, void*) noexcept { return nullptr; }

}  // namespace cube::profiling

#endif
