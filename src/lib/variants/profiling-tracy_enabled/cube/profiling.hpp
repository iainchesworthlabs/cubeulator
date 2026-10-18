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

inline void app_info(std::string_view text) {
    TracyAppInfo(text.data(), text.size());
}

// Tracy's plot call is overloaded on int64_t, float and double, so an int
// argument is ambiguous between all three. Route every value through the
// obvious one of the two Tracy keeps exact: integers stay integers, anything
// floating becomes a double.
template <typename T>
void plot(const char* name, T value) {
    static_assert(std::is_arithmetic_v<T>, "CUBE_PLOT takes a number");
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
        : context_(___tracy_emit_zone_begin(location, 1)) {}
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
enum class gpu_backend : unsigned { none = 0, opengl, d3d11 };

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
    virtual ~GpuContext() = default;

    [[nodiscard]] virtual bool valid() const noexcept = 0;
    // Inserts the zone's begin timestamp into the current GPU command
    // stream. `name` is a string literal: Tracy keeps the pointer.
    virtual void begin_zone(const char* name) noexcept = 0;
    // Inserts the zone's end timestamp. No effect if no zone is open.
    virtual void end_zone() noexcept = 0;
    // Resolves completed GPU timestamps; once per frame, after end_zone().
    virtual void collect() noexcept = 0;
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
// timeline by - the app's startup, one solve. `name` is a literal and must
// contain no printf conversion; the id returned is handed back to leave().
inline std::uint32_t section_enter(const char* name) {
    return tracy::Profiler::SectionEnter(0, name);
}

inline void section_leave(std::uint32_t id) {
    tracy::Profiler::SectionLeave(id);
}

}  // namespace cube::profiling

// Macros, not functions, on purpose: Tracy's zone is an object that must live
// in the caller's scope, its name must be a literal whose address it keeps,
// and the disabled sibling must be able to make every one of these vanish
// without evaluating its arguments.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// A zone covering the rest of the enclosing C++ scope, named after the
// function.
#define CUBE_ZONE_SCOPED() ZoneScoped
// The same, with an explicit name.
#define CUBE_ZONE_SCOPED_N(name) ZoneScopedN(name)
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
#define CUBE_ZONE_SCOPED_VALUE(number) ZoneValue(static_cast<std::uint64_t>(number))

// One per real-time frame, for the frame view. The unnamed set is the direct
// scan's per-frame pass; "Capture" (camera frames delivered) and "UI" (Qt Quick
// presentations) are named sets alongside it, so three unrelated cadences
// never interleave into one meaningless graph.
#define CUBE_FRAME_MARK() FrameMark
#define CUBE_FRAME_MARK_NAMED(name) FrameMarkNamed(name)

// A value on a timeline plot. `name` is a literal; `value` is an integer,
// floating-point or bool expression.
#define CUBE_PLOT(name, value) ::cube::profiling::plot(name, value)

// A message on the timeline, for a discrete event (scan complete, tracker
// lost). `text` is a literal.
#define CUBE_MESSAGE(text) TracyMessageL(text)

// Name the calling thread in the GUI's thread list. Once per thread: every
// call adds a table entry, so never put this in a per-frame or per-task path.
#define CUBE_THREAD_NAME(name) tracy::SetThreadName(name)

// Free text shown in the trace's information window (version, build).
#define CUBE_APP_INFO(text) ::cube::profiling::app_info(text)

// An execution section (Tracy 0.14): begin/end a named region the GUI can
// filter the timeline by. `name` is a literal with no printf conversion.
#define CUBE_SECTION_ENTER(name) ::cube::profiling::section_enter(name)
#define CUBE_SECTION_LEAVE(id) ::cube::profiling::section_leave(id)

// NOLINTEND(cppcoreguidelines-macro-usage)
