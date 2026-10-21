// Platforms that cannot ask cheaply whether a graphics backend will start (and
// whose Qt falls back or fails cleanly where it cannot): every backend is
// reported usable. Named by the macOS, iOS, Android and web fragments.
#include "interfaces/graphics_probe.hpp"

namespace cube::platform::graphics_probe {

bool usable(std::string_view /*backend*/) {
    return true;
}

}  // namespace cube::platform::graphics_probe
