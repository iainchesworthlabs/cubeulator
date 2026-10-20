#pragma once

// Tracy instrumentation macros, no-op variant - the default. See the
// profiling-tracy_enabled sibling's identically-pathed cube/profiling.hpp for
// the real-Tracy variant and for why CMake, not an #ifdef, selects between
// the two.
//
// Every macro expands to nothing, so instrumented source compiles as the
// source would with no instrumentation in it, and Tracy need not be installed
// at all. The corollary: an argument is NOT evaluated here. Keep the
// expression handed to CUBE_PLOT / CUBE_ZONE_VALUE free of side effects, and
// never let it be the only use of a variable (that is an unused-variable
// warning, an error under -Werror, in exactly the build nobody profiles).

#include <cstdint>

namespace cube::profiling {

inline constexpr bool enabled = false;
inline constexpr bool to_recorder = false;

// The run-time switch's surface (see the profiling-tracy_enabled sibling):
// nothing to start here, and never active.
[[nodiscard]] constexpr bool active() noexcept {
    return false;
}
inline void start() noexcept {}
inline void stop() noexcept {}

// The GPU (render-thread) surface, as no-ops: the profiling-tracy_enabled
// sibling implements these (cube::lib's src/profile/tracy_gpu.cpp); here
// they exist so the app's render thread compiles unchanged and nothing is
// created. See the sibling for the contract.
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

struct vulkan_gpu_handles {
    void* instance = nullptr;
    void* physical_device = nullptr;
    void* device = nullptr;
    void* queue = nullptr;
    std::uint32_t queue_family = 0;
    void* get_instance_proc_addr = nullptr;
    void* get_device_proc_addr = nullptr;
};

class GpuContext {
public:
    static GpuContext* create_opengl(const gl_gpu_functions& /*functions*/) noexcept {
        return nullptr;
    }
    static GpuContext* create_d3d11(void* /*device*/, void* /*device_context*/) noexcept {
        return nullptr;
    }
    static GpuContext* create_vulkan(const vulkan_gpu_handles& /*handles*/) noexcept {
        return nullptr;
    }
    GpuContext() = default;
    GpuContext(const GpuContext&) = delete;
    GpuContext& operator=(const GpuContext&) = delete;
    GpuContext(GpuContext&&) = delete;
    GpuContext& operator=(GpuContext&&) = delete;
    virtual ~GpuContext() = default;

    [[nodiscard]] virtual bool valid() const noexcept { return false; }
    virtual void begin_zone(const char* /*name*/, void* /*command_stream*/) noexcept {}
    virtual void end_zone() noexcept {}
    virtual void collect(void* /*command_stream*/) noexcept {}
};

class ZoneSpan {
public:
    void begin(const char* /*name*/) noexcept {}
    void end() noexcept {}
};

inline std::uint32_t section_enter(const char* /*name*/) {
    return 0;
}
inline void section_leave(std::uint32_t /*id*/) {}

// The value-with-no-open-zone path, kept callable in every variant so a test
// can drive it directly; here, as in the Tracy variant, nothing is open and
// nothing records the value (the recorder variant drops it through its
// zone stack).
inline void scoped_zone_value(std::uint64_t /*value*/) noexcept {}

}  // namespace cube::profiling

// Macros, not functions, on purpose: a function would evaluate its arguments
// and could not vanish, and the real variant needs the name to be a literal
// whose address Tracy keeps.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define CUBE_ZONE_SCOPED()
#define CUBE_ZONE_SCOPED_N(name)
#define CUBE_ZONE_BEGIN(var, name)
#define CUBE_ZONE_END(var)
#define CUBE_ZONE_VALUE(var, number)
#define CUBE_ZONE_SCOPED_VALUE(number)
#define CUBE_FRAME_MARK()
#define CUBE_FRAME_MARK_NAMED(name)
#define CUBE_PLOT(name, value)
#define CUBE_MESSAGE(text)
#define CUBE_MESSAGE_DYNAMIC(text)
#define CUBE_THREAD_NAME(name)
#define CUBE_APP_INFO(text)
#define CUBE_SECTION_ENTER(name) ::cube::profiling::section_enter(name)
#define CUBE_SECTION_LEAVE(id) ::cube::profiling::section_leave(id)
// NOLINTEND(cppcoreguidelines-macro-usage)
