#pragma once

// The window's graphics API: chosen at launch, and guarded so that the choice
// can never be what stops the application starting. The decision itself is
// cube/graphics/backend_choice.hpp (pure, unit-tested); this is the part that
// touches the process - the environment, QSettings, QQuickWindow, a relaunch.
// docs/graphics-api.md.

#include <cube/graphics/backend_choice.hpp>

#include <QObject>
#include <QSettings>
#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class QQuickWindow;

namespace cube::app::graphics {

// What the guard remembers between launches, under rendering/guard/ in the
// application's settings (the same store as the Settings screen's picker, which
// is rendering/graphicsBackend).
[[nodiscard]] cube::graphics::record load_record(const QSettings& settings);
void store_record(QSettings& settings, const cube::graphics::record& record);

// What identifies this build to the guard: a failure recorded by another
// version of the application, or on another Qt, is not held against this one.
[[nodiscard]] std::string build_identity();

// Whether a backend can start on this machine (interfaces/graphics_probe.hpp).
using Probe = std::function<bool(std::string_view backend)>;

// One launch's choice, applied. Create it before the QGuiApplication (the API
// can only be set until a window exists), call watch() with the window once
// there is one.
class Launch : public QObject {
    Q_OBJECT
public:
    // `settings` is the application's store (organisation and application name
    // already set); `argv_tail` is argv[1..]; `offered` is what the platform
    // supports (platform_info::current()) and `software_api` the key of the API
    // its "software" backend draws with. Each backend of the chain is asked of
    // `usable` in turn, and the first that says yes (or the last, which is
    // never asked) is the one set.
    Launch(std::unique_ptr<QSettings> settings, std::vector<std::string_view> argv_tail,
           cube::graphics::platform offered, std::string software_api, Probe usable);

    // The backend this launch set out with, "" when Qt was left to pick.
    [[nodiscard]] const std::string& backend() const noexcept { return backend_; }
    [[nodiscard]] const cube::graphics::choice& choice() const noexcept { return choice_; }

    // Confirms the backend at the first presented frame, and if the window
    // reports that its scene graph cannot be created before that, records the
    // failure and starts the application again, which then goes past it.
    void watch(QQuickWindow& window);

private:
    void confirm();
    void failed(const QString& reason);

    std::unique_ptr<QSettings> settings_;
    cube::graphics::choice choice_;
    std::vector<std::string_view> argv_tail_;
    std::vector<std::string> skipped_;
    std::string backend_;
    std::string setting_;  // the Settings picker's value at launch
    bool confirmed_ = false;
    bool relaunching_ = false;
};

}  // namespace cube::app::graphics
