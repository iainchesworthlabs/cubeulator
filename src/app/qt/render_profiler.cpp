#include "render_profiler.hpp"

#include <cube/profiling.hpp>

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include <cstring>

namespace cube::app {

namespace {

// glGetError/glGetIntegerv/glGetString are GL 1.1 entry points some loaders
// do not answer through getProcAddress; they go through the window's own
// QOpenGLFunctions instead. The trampolines exist once, for the one window
// the app opens; everything below runs on the render thread, where that
// context is current.
QOpenGLFunctions* gl1_fallback = nullptr;

GLenum fallback_get_error() { return gl1_fallback->glGetError(); }
void fallback_get_integerv(GLenum name, GLint* values) {
    gl1_fallback->glGetIntegerv(name, values);
}
const GLubyte* fallback_get_string(GLenum name) { return gl1_fallback->glGetString(name); }

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
    QObject::connect(&window, &QQuickWindow::beforeRendering, profiler,
                     &RenderProfiler::beforeRendering, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterRendering, profiler,
                     &RenderProfiler::afterRendering, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::frameSwapped, profiler,
                     &RenderProfiler::frameSwapped, Qt::DirectConnection);
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
        default:
            // Direct3D 12, Vulkan, Metal and software rendering: a GPU zone
            // cannot bracket the frame from outside the scene graph's own
            // command buffers (see docs/profiling.md). The CPU zone and the
            // frame-time plot still record.
            CUBE_MESSAGE("render.gpu_unavailable");
            break;
    }
    if (gpu_ != nullptr && !gpu_->valid()) {
        gpu_ = nullptr;
    }
}

void RenderProfiler::beforeRendering() {
    ensureGpuContext();
    frame_zone_.begin("render.frame");
    if (gpu_ != nullptr) {
        gpu_->begin_zone("render.gpu");
    }
}

void RenderProfiler::afterRendering() {
    if (gpu_ != nullptr) {
        gpu_->end_zone();
        gpu_->collect();
    }
    frame_zone_.end();
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
