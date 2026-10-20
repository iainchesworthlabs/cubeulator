#include "render_profiler.hpp"

#include <cube/profiling.hpp>

#include <QByteArray>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include <cstddef>
#include <cstring>
#include <string_view>

namespace cube::app {

namespace {

// glGetError/glGetIntegerv/glGetString are GL 1.1 entry points some loaders
// do not answer through getProcAddress; they go through the window's own
// QOpenGLFunctions instead. The trampolines exist once, for the one window
// the app opens; everything below runs on the render thread, where that
// context is current.
QOpenGLFunctions* gl1_fallback = nullptr;

GLenum fallback_get_error() {
    return gl1_fallback->glGetError();
}
void fallback_get_integerv(GLenum name, GLint* values) {
    gl1_fallback->glGetIntegerv(name, values);
}
const GLubyte* fallback_get_string(GLenum name) {
    return gl1_fallback->glGetString(name);
}

void* resolve(QOpenGLContext* context, const char* name) {
    if (QFunctionPointer pointer = context->getProcAddress(name); pointer != nullptr) {
        return reinterpret_cast<void*>(pointer);
    }
    if (std::strcmp(name, "glGetError") == 0) {
        return reinterpret_cast<void*>(&fallback_get_error);
    }
    if (std::strcmp(name, "glGetIntegerv") == 0) {
        return reinterpret_cast<void*>(&fallback_get_integerv);
    }
    if (std::strcmp(name, "glGetString") == 0) {
        return reinterpret_cast<void*>(&fallback_get_string);
    }
    return nullptr;
}

const char* graphicsApiName(QSGRendererInterface::GraphicsApi api) {
    switch (api) {
        case QSGRendererInterface::Software:
            return "software";
        case QSGRendererInterface::OpenVG:
            return "OpenVG";
        case QSGRendererInterface::OpenGL:
            return "OpenGL";
        case QSGRendererInterface::Direct3D11:
            return "Direct3D 11";
        case QSGRendererInterface::Vulkan:
            return "Vulkan";
        case QSGRendererInterface::Metal:
            return "Metal";
        case QSGRendererInterface::Direct3D12:
            return "Direct3D 12";
        case QSGRendererInterface::Null:
            return "null";
        case QSGRendererInterface::Unknown:
            break;
    }
    return "unknown";
}

}  // namespace

RenderProfiler::RenderProfiler(QQuickWindow& window) : QObject(nullptr), window_(window) {
    clock_.start();
}

RenderProfiler* RenderProfiler::install(QQuickWindow& window) {
    auto* profiler = new RenderProfiler(window);
    profiler->setParent(&window);
    // Direct, not queued: frameSwapped and the render brackets can fire on
    // the scene graph's render thread, and the zones must timestamp what
    // they bracket, not a queued copy of it.
    QObject::connect(&window, &QQuickWindow::sceneGraphInitialized, profiler,
                     &RenderProfiler::sceneGraphInitialized, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::sceneGraphInvalidated, profiler,
                     &RenderProfiler::sceneGraphInvalidated, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::beforeSynchronizing, profiler,
                     &RenderProfiler::beforeSynchronizing, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterSynchronizing, profiler,
                     &RenderProfiler::afterSynchronizing, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::beforeFrameBegin, profiler,
                     &RenderProfiler::beforeFrameBegin, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterFrameEnd, profiler,
                     &RenderProfiler::afterFrameEnd, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::beforeRendering, profiler,
                     &RenderProfiler::beforeRendering, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterRendering, profiler,
                     &RenderProfiler::afterRendering, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::beforeRenderPassRecording, profiler,
                     &RenderProfiler::beforeRenderPassRecording, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterRenderPassRecording, profiler,
                     &RenderProfiler::afterRenderPassRecording, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::frameSwapped, profiler, &RenderProfiler::frameSwapped,
                     Qt::DirectConnection);
    return profiler;
}

void RenderProfiler::ensureGpuContext() noexcept {
    if (tried_gpu_) {
        return;
    }
    tried_gpu_ = true;

    QSGRendererInterface* renderer = window_.rendererInterface();
    if (renderer == nullptr) {
        return;
    }

    // The trace says which graphics backend it measured: it is chosen at
    // launch and changes what every number below means.
    const QByteArray backend =
        QByteArray("graphics backend: ") + graphicsApiName(renderer->graphicsApi());
    [[maybe_unused]] const std::string_view backend_view{backend.constData(),
                                                         static_cast<std::size_t>(backend.size())};
    CUBE_APP_INFO(backend_view);

    switch (renderer->graphicsApi()) {
        case QSGRendererInterface::OpenGL: {
            auto* context = static_cast<QOpenGLContext*>(
                renderer->getResource(&window_, QSGRendererInterface::OpenGLContextResource));
            if (context == nullptr) {
                return;
            }
            gl1_fallback = context->functions();
            ::cube::profiling::gl_gpu_functions functions;
            functions.gen_queries = resolve(context, "glGenQueries");
            functions.get_queryiv = resolve(context, "glGetQueryiv");
            functions.get_query_objectiv = resolve(context, "glGetQueryObjectiv");
            functions.get_query_objectui64v = resolve(context, "glGetQueryObjectui64v");
            functions.get_integer64v = resolve(context, "glGetInteger64v");
            functions.query_counter = resolve(context, "glQueryCounter");
            functions.get_error = resolve(context, "glGetError");
            functions.get_integerv = resolve(context, "glGetIntegerv");
            functions.get_string = resolve(context, "glGetString");
            functions.get_stringi = resolve(context, "glGetStringi");
            gpu_ = ::cube::profiling::GpuContext::create_opengl(functions);
            break;
        }
        case QSGRendererInterface::Direct3D11: {
            void* device = renderer->getResource(&window_, QSGRendererInterface::DeviceResource);
            void* device_context =
                renderer->getResource(&window_, QSGRendererInterface::DeviceContextResource);
            gpu_ = ::cube::profiling::GpuContext::create_d3d11(device, device_context);
            break;
        }
        case QSGRendererInterface::Vulkan:
            gpu_ = createVulkanGpuContext(window_);
            gpu_needs_command_list_ = true;
            break;
        default:
            // Direct3D 12, Metal and software rendering: no GPU zone (see
            // docs/profiling.md for why, backend by backend). The CPU zones
            // and the frame-time plot still record.
            break;
    }
    if (gpu_ != nullptr && !gpu_->valid()) {
        gpu_ = nullptr;
    }
    if (gpu_ == nullptr) {
        CUBE_MESSAGE("render.gpu_unavailable");
    }
}

RenderProfiler::ExternalCommands::ExternalCommands(QQuickWindow& window, bool needed)
    : window_(needed ? &window : nullptr) {
    if (window_ != nullptr) {
        window_->beginExternalCommands();
    }
}

RenderProfiler::ExternalCommands::~ExternalCommands() {
    if (window_ != nullptr) {
        window_->endExternalCommands();
    }
}

void* RenderProfiler::commandStream() const {
    return vulkanCommandBuffer(window_);
}

void RenderProfiler::beginGpuZone() noexcept {
    if (gpu_ == nullptr || gpu_zone_open_) {
        return;
    }
    void* stream = gpu_needs_command_list_ ? commandStream() : nullptr;
    if (gpu_needs_command_list_ && stream == nullptr) {
        return;
    }
    const ExternalCommands native(window_, !gpu_needs_command_list_);
    gpu_->begin_zone("render.gpu", stream);
    gpu_zone_open_ = true;
}

void RenderProfiler::endGpuZone() noexcept {
    if (gpu_ == nullptr || !gpu_zone_open_) {
        return;
    }
    const ExternalCommands native(window_, !gpu_needs_command_list_);
    gpu_->end_zone();
    gpu_zone_open_ = false;
}

void RenderProfiler::sceneGraphInitialized() {
    CUBE_MESSAGE("render.scenegraph_initialized");
}

void RenderProfiler::sceneGraphInvalidated() {
    CUBE_MESSAGE("render.scenegraph_invalidated");
}

void RenderProfiler::beforeSynchronizing() {
    sync_zone_.begin("render.sync");
}

void RenderProfiler::afterSynchronizing() {
    sync_zone_.end();
}

void RenderProfiler::beforeFrameBegin() {
    rhi_frame_zone_.begin("render.rhi_frame");
}

void RenderProfiler::afterFrameEnd() {
    rhi_frame_zone_.end();
}

void RenderProfiler::beforeRendering() {
    ensureGpuContext();
    frame_zone_.begin("render.frame");
    // Resolve the timestamps the GPU has finished, before this frame's zone: a
    // Vulkan context records the reset of what it has just read into the
    // frame's buffer, which is outside a render pass only here.
    if (gpu_ != nullptr) {
        gpu_->collect(gpu_needs_command_list_ ? commandStream() : nullptr);
    }
}

void RenderProfiler::afterRendering() {
    frame_zone_.end();
}

// The GPU zone is the scene graph's main render pass, begun and ended with the
// pass's own signals. It cannot be the pair beforeRendering/afterRendering:
// every backend but OpenGL and Direct3D 11 hands out a command buffer that is
// only valid inside the pass (Vulkan's is a secondary buffer, not the frame's),
// and those two RHI backends record their commands and replay them when the
// pass ends, so a timestamp issued from beforeRendering would run before the
// frame's commands, not around them. Inside the pass, beginExternalCommands()
// first replays what has been recorded, so the immediate-context timestamp
// lands in order.
void RenderProfiler::beforeRenderPassRecording() {
    pass_zone_.begin("render.pass");
    beginGpuZone();
}

void RenderProfiler::afterRenderPassRecording() {
    endGpuZone();
    pass_zone_.end();
}

void RenderProfiler::frameSwapped() {
    // The "UI" frame set: one mark per Qt Quick presentation, timestamped at
    // the swap itself. The unnamed set is the vision pass and "Capture" the
    // camera - three unrelated cadences, three sets.
    CUBE_FRAME_MARK_NAMED("UI");

    // The presentation cadence as a plot; the first swap only arms the
    // clock.
    const std::int64_t now = clock_.nsecsElapsed();
    if (last_swap_ns_ != 0) {
        CUBE_PLOT("render.frame_ms", static_cast<double>(now - last_swap_ns_) / 1e6);
    }
    last_swap_ns_ = now;
}

}  // namespace cube::app
