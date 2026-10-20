#pragma once

#include <string>
#include <vector>

namespace cube::platform::platform_info {

// One graphics API the Settings screen offers beyond "auto". `key` is the
// value that is persisted and that main.cpp's applyPersistedGraphicsBackend()
// accepts ("d3d11", "d3d12", "vulkan", "opengl", "metal"); `label` is how
// the screen words it.
struct GraphicsBackend {
    std::string key;
    std::string label;
};

// What the QML scene needs to know about the platform it is running on.
// Everything here used to be a Qt.platform.os test in QML, which meant a new
// platform needed a QML edit; it is data the platform supplies now.
struct PlatformInfo {
    // A phone: the OS makes the app ask for camera permission itself, in-app,
    // before the first device is opened, and there is one camera to use, so
    // the screens ask about permission rather than offering a device choice.
    // On the desktops the OS prompts the first time a device is opened.
    bool is_mobile = false;

    // The camera list is not trustworthy until a camera has been opened: a
    // browser hides every camera's id and name from a page that has not been
    // given camera access, so the list read at launch stands for "the rear
    // camera" and "the front camera" rather than for devices. The app lists the
    // cameras again once one is open, and a selection that no longer matches
    // anything is carried over to the camera that faces the same way.
    bool camera_ids_withheld_until_use = false;

    // The scan screen offers a button to change camera, when there is more than
    // one: the cameras are the ones on a phone (front and back), where the
    // person is holding the device and cannot be sent to a settings screen to
    // turn the picture around.
    bool camera_switch_on_scan_screen = false;

    // The QRhi backends Qt actually supports here (see
    // https://doc.qt.io/qt-6/qsgrendererinterface.html), in the order the
    // Settings screen lists them. Direct3D is Windows-only, Metal is
    // Apple-only, Vulkan/OpenGL are available wherever Qt ships a backend for
    // them.
    std::vector<GraphicsBackend> graphics_backends;

    // The families, in preference order, that this platform's own interface
    // text is set in when the app's language is Japanese or Simplified
    // Chinese. The two share code points and not shapes, so Theme.qml names a
    // family for them and does not leave it to glyph fallback; the first one
    // that is installed wins. Each platform lists what its OS ships; the
    // browser lists the faces the app bundles and registers (it has no system
    // fonts). Empty means "the default face".
    std::vector<std::string> japanese_faces;
    std::vector<std::string> chinese_faces;

    // Qt's font database may be asked for the interface's families on a thread
    // of its own while the screens are built (kit/font_warmup.hpp). Where a
    // family the machine does not have is looked for in every font it has, that
    // search is a third of a second the first text would otherwise wait for.
    // False where there is nothing to search and a thread is not free to make:
    // a browser's threads are Web Workers, started by the page's event loop.
    bool warm_fonts_off_thread = true;
};

// There is exactly one definition of `current()` per platform, living in
// platform/<os>/platform_info_<os>.cpp. CMake compiles only the file the
// platform's fragment names, so the code contains no preprocessor branching.
[[nodiscard]] PlatformInfo current();

}  // namespace cube::platform::platform_info
