#pragma once

#include <cube/profiling.hpp>

#include <QElapsedTimer>
#include <QObject>

#include <cstdint>

class QQuickWindow;

namespace cube::app {

// Binds Tracy's render-side instrumentation to one QQuickWindow: a CPU zone
// and - where the graphics backend supports it (OpenGL, Direct3D 11) - a
// GPU zone around each presented frame, the per-frame duration plot, and
// the "UI" frame mark. Installed from main.cpp.
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
    void beforeRendering();
    void afterRendering();
    void frameSwapped();

    QQuickWindow& window_;
    ::cube::profiling::GpuContext* gpu_ = nullptr;
    ::cube::profiling::ZoneSpan frame_zone_;
    bool tried_gpu_ = false;
    QElapsedTimer clock_;
    std::int64_t last_swap_ns_ = 0;
};

}  // namespace cube::app
