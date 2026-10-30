// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cube/profiling.hpp>

#include <QElapsedTimer>
#include <QObject>

#include <cstdint>

class QQuickWindow;

namespace cube::app {

// The Vulkan half of the render profiler, in its own translation unit because
// it needs the Vulkan headers Qt's QVulkanInstance is built on (see
// qt/render_profiler_vulkan.cpp; the platforms without Vulkan get the stub in
// render_profiler_vulkan_none.cpp, which finds nothing).
//
// createVulkanGpuContext builds Tracy's Vulkan GPU context from the objects
// the window's scene graph renders with; null if they cannot be found.
// vulkanCommandBuffer is the VkCommandBuffer the scene graph is recording this
// frame, as an opaque handle, or null when Qt has none to give just now.
// Both run on the render thread.
[[nodiscard]] ::cube::profiling::GpuContext* createVulkanGpuContext(QQuickWindow& window);
[[nodiscard]] void* vulkanCommandBuffer(QQuickWindow& window);

// Binds Tracy's render-side instrumentation to one QQuickWindow: CPU zones for
// the scene graph's phases (synchronisation, the RHI frame, the render and its
// main pass), where the graphics backend supports it (Vulkan, Direct3D 11,
// OpenGL) a GPU zone around the main render pass, the per-frame duration
// plot, and the "UI" frame mark. Installed from main.cpp, only when the
// profiler is running (cube::profiling::active()).
//
// The handlers are connected directly, so they run on the scene graph's
// render thread, which is where a Tracy GPU context must live (the graphics
// context is current and the command stream is the one being drawn there).
// Zones therefore pair correctly whatever loop the window uses.
class RenderProfiler : public QObject {
    Q_OBJECT

public:
    // Connects to `window` and parents the profiler to it; returns the new
    // object, owned by the window.
    static RenderProfiler* install(QQuickWindow& window);

private:
    explicit RenderProfiler(QQuickWindow& window);

    void ensureGpuContext() noexcept;
    void sceneGraphInitialized();
    void sceneGraphInvalidated();
    void beforeSynchronizing();
    void afterSynchronizing();
    void beforeFrameBegin();
    void afterFrameEnd();
    void beforeRendering();
    void afterRendering();
    void beforeRenderPassRecording();
    void afterRenderPassRecording();
    void frameSwapped();

    // The native command buffer the render pass is being recorded into, for
    // the backends whose GPU timestamps are commands (Vulkan); null when Qt
    // has none to give at this point.
    [[nodiscard]] void* commandStream() const;
    void beginGpuZone() noexcept;
    void endGpuZone() noexcept;

    // Brackets native graphics calls made from a render-pass signal on the
    // backends that record Qt's commands and replay them later (OpenGL,
    // Direct3D 11): QQuickWindow::beginExternalCommands() replays what has
    // been recorded so far, so a native call lands in order. A no-op for the
    // backends whose timestamps are recorded into the command buffer itself.
    class ExternalCommands {
    public:
        ExternalCommands(QQuickWindow& window, bool needed);
        ~ExternalCommands();
        ExternalCommands(const ExternalCommands&) = delete;
        ExternalCommands& operator=(const ExternalCommands&) = delete;
        ExternalCommands(ExternalCommands&&) = delete;
        ExternalCommands& operator=(ExternalCommands&&) = delete;

    private:
        QQuickWindow* window_;
    };

    QQuickWindow& window_;
    ::cube::profiling::GpuContext* gpu_ = nullptr;
    // Whether this window's GPU timestamps are commands recorded into the
    // scene graph's command buffer (Vulkan), as opposed to calls on an
    // implicit context (OpenGL, Direct3D 11).
    bool gpu_needs_command_list_ = false;
    bool gpu_zone_open_ = false;
    ::cube::profiling::ZoneSpan sync_zone_;
    ::cube::profiling::ZoneSpan rhi_frame_zone_;
    ::cube::profiling::ZoneSpan frame_zone_;
    ::cube::profiling::ZoneSpan pass_zone_;
    bool tried_gpu_ = false;
    QElapsedTimer clock_;
    std::int64_t last_swap_ns_ = 0;
};

}  // namespace cube::app
