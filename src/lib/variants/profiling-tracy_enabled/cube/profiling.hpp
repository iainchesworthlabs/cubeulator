#pragma once

// Tracy instrumentation macros, real-Tracy variant. CMake - not an #ifdef -
// selects this file over the profiling-tracy_disabled sibling: when
// CUBEULATOR_ENABLE_TRACY is on, src/lib/CMakeLists.txt puts this directory
// on cube::lib's public include path (see cmake/Tracy.cmake). Exactly one of
// the two is ever compiled, both at this same "cube/profiling.hpp" relative
// path, so every `#include <cube/profiling.hpp>` and every CUBE_ZONE_* use
// compiles unchanged either way.
//
// Zone names are string LITERALS: Tracy keeps the pointer, not a copy. For
// the same reason the name given to CUBE_PLOT, CUBE_FRAME_MARK_NAMED and
// CUBE_MESSAGE must outlive the process - never pass a temporary.
//
// The client is built for MANUAL LIFETIME (the overlay tracy port in vcpkg-overlay-ports/tracy):
// nothing is started, listened on or broadcast until the application calls
// start() - which cube::profile::Session does, once, from main(), when the
// build's default or the run-time switch (CUBEULATOR_PROFILE, --profile) says
// so. Before that - and in every process that never asks, which is a release
// build in the field - Tracy's own state does not exist, so every macro below
// first checks active(), one relaxed atomic load, and does nothing. Tracy's
// on-demand mode then spares the work while no profiler is attached to a
// running client.

#include <atomic>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

#include <tracy/TracyC.h>
#include <tracy/Tracy.hpp>

namespace cube::profiling {

// True in this variant, false in the sibling. A test pins it against the
// CMake option so the wrong directory cannot be selected unnoticed.
inline constexpr bool enabled = true;
// False here: this variant hands events to Tracy, not to the in-process
// recorder (the profiling-recorder sibling).
inline constexpr bool to_recorder = false;

namespace detail {
inline std::atomic<bool> running{false};
}  // namespace detail

// True once start() has run. Every entry point of this header that reaches
// Tracy asks first, because with manual lifetime Tracy's state does not exist
// until then.
[[nodiscard]] inline bool active() noexcept {
    return detail::running.load(std::memory_order_relaxed);
}

// Starts the Tracy client: its worker thread, the loopback listener and the
// discovery broadcast. Call once, before the threads that should be profiled
// start (a thread names itself with CUBE_THREAD_NAME as it begins). Idempotent
// on one thread; not meant to race with itself.
inline void start() noexcept {
    if (!detail::running.load(std::memory_order_acquire)) {
        tracy::StartupProfiler();
        detail::running.store(true, std::memory_order_release);
    }
}

// Stops the client and flushes what it holds. Only when no other thread can
// be inside a zone any more: cube::profile::Session calls it from main(), after every
// worker is joined.
inline void stop() noexcept {
    if (detail::running.exchange(false, std::memory_order_acq_rel)) {
        tracy::ShutdownProfiler();
    }
}

inline void app_info(std::string_view text) {
    if (active()) {
        TracyAppInfo(text.data(), text.size());
    }
}

// A message with text built at run time; Tracy copies it.
inline void message_dynamic(std::string_view text) {
    if (active()) {
        TracyMessage(text.data(), text.size());
    }
}

// The literal-name entry points behind CUBE_MESSAGE / CUBE_FRAME_MARK*.
inline void message_literal(const char* text) {
    if (active()) {
        TracyMessageL(text);
    }
}

inline void frame_mark(const char* name) {
    if (active()) {
        tracy::Profiler::SendFrameMark(name);
    }
}

inline void thread_name(const char* name) {
    if (active()) {
        tracy::SetThreadName(name);
    }
}

// Tracy's plot call is overloaded on int64_t, float and double, so an int
// argument is ambiguous between all three. Route every value through the
// obvious one of the two Tracy keeps exact: integers stay integers, anything
// floating becomes a double.
template <typename T>
void plot(const char* name, T value) {
    static_assert(std::is_arithmetic_v<T>, "CUBE_PLOT takes a number");
    if (!active()) {
        return;
    }
    if constexpr (std::is_floating_point_v<T>) {
        TracyPlot(name, static_cast<double>(value));
    } else {
        TracyPlot(name, static_cast<std::int64_t>(value));
    }
}

// The value-with-no-open-zone path, kept callable in every variant so a test
// can drive it directly: in this variant CUBE_ZONE_SCOPED_VALUE is
// ZoneValue, which cannot even compile without a CUBE_ZONE_SCOPED*() zone in
// scope - exactly what a test for "no zone open" must not have. Here nothing
// is open and nothing records the value.
inline void scoped_zone_value(std::uint64_t) noexcept {}

// The manual (begin ... end) zone, made exception safe.
//
// Tracy's own C-API pair (TracyCZoneN / TracyCZoneEnd) leaves the zone open
// when an exception unwinds past the end call, and an unclosed zone nests
// every later zone on that thread beneath it for the rest of the trace. This
// app catches exceptions around its analysis pass on purpose (a degenerate
// frame must drop one frame, not the scan), so the pair has to survive that:
// the destructor closes the zone if nothing else did. end() is idempotent.
class ManualZone {
public:
    explicit ManualZone(const ___tracy_source_location_data* location) noexcept
        : context_(___tracy_emit_zone_begin(location, active() ? 1 : 0)) {}
    ~ManualZone() { end(); }

    ManualZone(const ManualZone&) = delete;
    ManualZone& operator=(const ManualZone&) = delete;
    ManualZone(ManualZone&&) = delete;
    ManualZone& operator=(ManualZone&&) = delete;

    void value(std::uint64_t number) const noexcept {
        if (open_) {
            ___tracy_emit_zone_value(context_, number);
        }
    }

    void end() noexcept {
        if (open_) {
            open_ = false;
            ___tracy_emit_zone_end(context_);
        }
    }

private:
    TracyCZoneCtx context_;
    bool open_ = true;
};

// ---- GPU (render-thread) zones ------------------------------------------
//
// The Tracy GPU context and one open GPU zone, behind an opaque interface:
// the real implementations need Tracy's per-backend bindings and, on
// Windows, <d3d11.h>, which no other translation unit should pay for - they
// live in cube::lib's src/profile/tracy_gpu.cpp, built only when Tracy is
// on. The disabled and recorder variants declare the same classes as
// no-ops, so the app's render thread compiles unchanged whatever the build.
//
// A context must be created and driven on the thread whose command stream
// it brackets - the scene graph's render thread - and begin_zone()/end_zone()
// insert timestamps around whatever is drawn between the two calls.

// The OpenGL entry points Tracy's GL context calls, as the app resolves
// them from its QOpenGLContext. All ten must be non-null for create_opengl
// to profile.
struct gl_gpu_functions {
    void* gen_queries = nullptr;
    void* get_queryiv = nullptr;
    void* get_query_objectiv = nullptr;
    void* get_query_objectui64v = nullptr;
    void* get_integer64v = nullptr;
    void* query_counter = nullptr;
    void* get_error = nullptr;
    void* get_integerv = nullptr;
    void* get_string = nullptr;
    void* get_stringi = nullptr;
};

// The Vulkan objects the scene graph renders with, as the application finds
// them (QSGRendererInterface) and the loader's two entry points, which Tracy
// resolves everything else through - so no Vulkan loader is linked here.
struct vulkan_gpu_handles {
    void* instance = nullptr;                // VkInstance
    void* physical_device = nullptr;         // VkPhysicalDevice
    void* device = nullptr;                  // VkDevice
    void* queue = nullptr;                   // VkQueue, the one the frame is submitted on
    std::uint32_t queue_family = 0;          // that queue's family index
    void* get_instance_proc_addr = nullptr;  // PFN_vkGetInstanceProcAddr
    void* get_device_proc_addr = nullptr;    // PFN_vkGetDeviceProcAddr
};

class GpuContext {
public:
    // OpenGL: constructs Tracy's per-thread GL context, which requires the
    // window's GL context to be current. One per thread; a second call
    // returns the same one.
    static GpuContext* create_opengl(const gl_gpu_functions& functions) noexcept;
    // Direct3D 11: `device` (ID3D11Device*) and `device_context`
    // (ID3D11DeviceContext*; null asks the device for its immediate
    // context). Returns null if no device is available.
    static GpuContext* create_d3d11(void* device, void* device_context) noexcept;
    // Vulkan: zones are timestamps written into the VkCommandBuffer handed to
    // begin_zone()/end_zone(); collect() needs the frame's buffer too, and
    // records into it, so it must be called outside a render pass. Returns
    // null if any handle is missing or the platform has no Vulkan backend.
    static GpuContext* create_vulkan(const vulkan_gpu_handles& handles) noexcept;
    GpuContext() = default;
    GpuContext(const GpuContext&) = delete;
    GpuContext& operator=(const GpuContext&) = delete;
    GpuContext(GpuContext&&) = delete;
    GpuContext& operator=(GpuContext&&) = delete;
    virtual ~GpuContext() = default;

    [[nodiscard]] virtual bool valid() const noexcept = 0;
    // Inserts the zone's begin timestamp into the GPU command stream.
    // `name` is a string literal: Tracy keeps the pointer. `command_stream`
    // is the native command buffer (VkCommandBuffer) for the backends whose
    // timestamps are commands recorded into one; OpenGL and Direct3D 11 issue
    // theirs on the context's implicit stream and ignore it.
    virtual void begin_zone(const char* name, void* command_stream) noexcept = 0;
    // Inserts the zone's end timestamp, into the same stream the zone began
    // in. No effect if no zone is open.
    virtual void end_zone() noexcept = 0;
    // Resolves completed GPU timestamps; once per frame. `command_stream` is
    // the frame's command buffer for the backends that must record the reset
    // of what they have just read into it (Vulkan), and null-safe for the
    // rest. Call it where no render pass is open, before the frame's zone.
    virtual void collect(void* command_stream) noexcept = 0;
};

// A CPU zone whose begin and end are not one C++ scope - the render-frame
// zone opens in beforeRendering and closes in afterRendering. The same
// exception safety as ManualZone: the destructor closes an open zone, and
// end() is idempotent.
class ZoneSpan {
public:
    ZoneSpan() = default;
    ~ZoneSpan() { end(); }
    ZoneSpan(const ZoneSpan&) = delete;
    ZoneSpan& operator=(const ZoneSpan&) = delete;
    ZoneSpan(ZoneSpan&&) = delete;
    ZoneSpan& operator=(ZoneSpan&&) = delete;

    void begin(const char* name) noexcept {
        if (zone_) {
            return;
        }
        location_ = ___tracy_source_location_data{name, __func__, __FILE__, 0, 0};
        zone_.emplace(&location_);
    }

    void end() noexcept {
        if (zone_) {
            zone_->end();
            zone_.reset();
        }
    }

private:
    ___tracy_source_location_data location_{};
    std::optional<ManualZone> zone_;
};

// Execution sections (Tracy 0.14): named regions the GUI can filter the
// timeline by - the app's startup, one solve. Tracy copies the name; the id
// returned is handed back to leave().
inline std::uint32_t section_enter(const char* name) {
    // The name goes through "%s": SectionEnter takes a printf format, and a
    // name that is data must not be read as one.
    return active() ? tracy::Profiler::SectionEnter(0, "%s", name) : 0;
}

inline void section_leave(std::uint32_t id) {
    if (active()) {
        tracy::Profiler::SectionLeave(id);
    }
}

}  // namespace cube::profiling

// Macros, not functions, on purpose: Tracy's zone is an object that must live
// in the caller's scope, its name must be a literal whose address it keeps,
// and the disabled sibling must be able to make every one of these vanish
// without evaluating its arguments.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// A zone covering the rest of the enclosing C++ scope, named after the
// function.
#define CUBE_ZONE_SCOPED() \
    SuppressVarShadowWarning(ZoneNamed(cube_scoped_zone, ::cube::profiling::active()))
// The same, with an explicit name.
#define CUBE_ZONE_SCOPED_N(name) \
    SuppressVarShadowWarning(ZoneNamedN(cube_scoped_zone, name, ::cube::profiling::active()))
// A manual (not lexically scoped) begin/end pair, for a span that does not
// correspond to one C++ scope - e.g. one stage inside an already-large
// function that should not be restructured into blocks just to give each
// stage its own. `var` names the zone the two calls share. Prefer the scoped
// form wherever a block is natural; this one exists for the stages-in-one-
// function shape.
#define CUBE_ZONE_BEGIN(var, name)                                          \
    static constexpr ___tracy_source_location_data var##_location{          \
        name, __func__, __FILE__, static_cast<std::uint32_t>(__LINE__), 0}; \
    ::cube::profiling::ManualZone var(&var##_location)
#define CUBE_ZONE_END(var) var.end()
// Attach an unsigned number to a zone (shown beside it in the GUI and exported
// by csvexport) - a count, a size, a pass number. The first form is for a
// manual zone and names its variable; the second is for the zone the
// enclosing CUBE_ZONE_SCOPED*() opened.
// (The parameter is `number`, not `value`: a parameter named `value` would
// also replace the `.value` member this expands to.)
#define CUBE_ZONE_VALUE(var, number) var.value(static_cast<std::uint64_t>(number))
#define CUBE_ZONE_SCOPED_VALUE(number) cube_scoped_zone.Value(static_cast<std::uint64_t>(number))

// One per real-time frame, for the frame view. The unnamed set is the direct
// scan's per-frame pass; "Capture" (camera frames delivered) and "UI" (Qt Quick
// presentations) are named sets alongside it, so three unrelated cadences
// never interleave into one meaningless graph.
#define CUBE_FRAME_MARK() ::cube::profiling::frame_mark(nullptr)
#define CUBE_FRAME_MARK_NAMED(name) ::cube::profiling::frame_mark(name)

// A value on a timeline plot. `name` is a literal; `value` is an integer,
// floating-point or bool expression.
#define CUBE_PLOT(name, value) ::cube::profiling::plot(name, value)

// A message on the timeline, for a discrete event (scan complete, tracker
// lost). `text` is a literal.
#define CUBE_MESSAGE(text) ::cube::profiling::message_literal(text)
// The same for text built at run time (Qt's own log lines, a device name):
// Tracy copies the characters, so a temporary is fine.
#define CUBE_MESSAGE_DYNAMIC(text) ::cube::profiling::message_dynamic(text)

// Name the calling thread in the GUI's thread list. Once per thread: every
// call adds a table entry, so never put this in a per-frame or per-task path.
#define CUBE_THREAD_NAME(name) ::cube::profiling::thread_name(name)

// Free text shown in the trace's information window (version, build).
#define CUBE_APP_INFO(text) ::cube::profiling::app_info(text)

// An execution section (Tracy 0.14): begin/end a named region the GUI can
// filter the timeline by. Tracy copies `name`.
#define CUBE_SECTION_ENTER(name) ::cube::profiling::section_enter(name)
#define CUBE_SECTION_LEAVE(id) ::cube::profiling::section_leave(id)

// NOLINTEND(cppcoreguidelines-macro-usage)
