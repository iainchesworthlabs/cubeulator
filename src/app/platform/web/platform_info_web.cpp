#include "interfaces/platform_info.hpp"

namespace cube::platform::platform_info {

PlatformInfo current() {
    // Not is_mobile, though a phone's browser runs this too: that flag means
    // "the app asks for camera permission itself, in-app, before the first
    // device is opened", and in a browser it is the opposite - the page may
    // not ask in advance, the browser prompts when getUserMedia() is first
    // called, as the desktops' OSes do when a device is first opened. The
    // device choice the desktop screens offer is what a browser has too:
    // a laptop has one webcam, a phone a front and a back one (see
    // devices_web.hpp for how those are listed).
    //
    // Qt on WebAssembly draws with WebGL, which is its OpenGL ES backend;
    // there is nothing else for the Settings screen to offer.
    return PlatformInfo{
        .is_mobile = false,
        .camera_ids_withheld_until_use = true,
        .camera_switch_on_scan_screen = true,
        .graphics_backends = {{.key = "opengl", .label = "WebGL"}},
        // A browser has no system fonts: these are the faces the app bundles
        // and registers (platform/web/fonts, main.cpp), the cut-down Noto
        // Sans JP and SC.
        .japanese_faces = {"Noto Sans JP"},
        .chinese_faces = {"Noto Sans SC"},
    };
}

}  // namespace cube::platform::platform_info
