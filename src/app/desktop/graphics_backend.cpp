#include "desktop/graphics_backend.hpp"

#include "interfaces/desktop_shell.hpp"

#include <cube/version.hpp>

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStringList>

#include <cstddef>
#include <optional>
#include <utility>

namespace cube::app::graphics {

namespace {

constexpr auto kSetting = "rendering/graphicsBackend";
constexpr auto kPending = "rendering/guard/pending";
constexpr auto kSuspect = "rendering/guard/suspect";
constexpr auto kStrikes = "rendering/guard/strikes";
constexpr auto kFailed = "rendering/guard/failed";
constexpr int kFailureFields = 4;  // backend, build, when, setting

constexpr QLatin1Char kField('\t');

std::optional<std::string> variable(const char* name) {
    if (!qEnvironmentVariableIsSet(name)) {
        return std::nullopt;
    }
    return qEnvironmentVariable(name).toStdString();
}

std::optional<QSGRendererInterface::GraphicsApi> api_for(std::string_view key) {
    if (key == "d3d11") {
        return QSGRendererInterface::Direct3D11;
    }
    if (key == "d3d12") {
        return QSGRendererInterface::Direct3D12;
    }
    if (key == "vulkan") {
        return QSGRendererInterface::Vulkan;
    }
    if (key == "opengl") {
        return QSGRendererInterface::OpenGL;
    }
    if (key == "metal") {
        return QSGRendererInterface::Metal;
    }
    return std::nullopt;
}

// Qt prefers the adapter that is a software rasteriser (WARP on Direct3D 11,
// a CPU device on Vulkan) when this is set; read when the first window's
// scene graph is created.
void prefer_software_rasteriser() {
    qputenv("QSG_RHI_PREFER_SOFTWARE_RENDERER", "1");
}

}  // namespace

cube::graphics::record load_record(const QSettings& settings) {
    cube::graphics::record record;
    record.pending = settings.value(kPending).toString().toStdString();
    record.suspect = settings.value(kSuspect).toString().toStdString();
    record.strikes = settings.value(kStrikes, 0).toInt();
    for (const QString& entry : settings.value(kFailed).toStringList()) {
        const QStringList fields = entry.split(kField);
        if (fields.size() != kFailureFields) {
            continue;
        }
        bool numeric = false;
        const qint64 when = fields[2].toLongLong(&numeric);
        if (!numeric || fields[0].isEmpty()) {
            continue;
        }
        record.failed.push_back({.backend = fields[0].toStdString(),
                                 .build = fields[1].toStdString(),
                                 .when = when,
                                 .setting = fields[3].toStdString()});
    }
    return record;
}

void store_record(QSettings& settings, const cube::graphics::record& record) {
    settings.setValue(kPending, QString::fromStdString(record.pending));
    settings.setValue(kSuspect, QString::fromStdString(record.suspect));
    settings.setValue(kStrikes, record.strikes);
    QStringList failed;
    for (const auto& f : record.failed) {
        failed.append(QString::fromStdString(f.backend) + kField + QString::fromStdString(f.build) +
                      kField + QString::number(f.when) + kField +
                      QString::fromStdString(f.setting));
    }
    settings.setValue(kFailed, failed);
    settings.sync();
}

std::string build_identity() {
    return std::string(cube::version_string) + " " + std::string(cube::git_commit) + " / Qt " +
           qVersion();
}

Launch::Launch(std::unique_ptr<QSettings> settings, std::vector<std::string_view> argv_tail,
               cube::graphics::platform offered, std::string software_api, Probe usable)
    : settings_(std::move(settings)), argv_tail_(std::move(argv_tail)) {
    const auto arguments = cube::graphics::parse_arguments(argv_tail_);
    skipped_ = arguments.skip;

    cube::graphics::request asked;
    asked.command_line = arguments.graphics_api;
    asked.environment = variable(std::string(cube::graphics::environment_variable).c_str());
    asked.qt_environment = variable(std::string(cube::graphics::qt_environment_variable).c_str());
    asked.setting = settings_->value(kSetting, "auto").toString().toStdString();
    setting_ = asked.setting;
    asked.skip = arguments.skip;

    const std::int64_t now = QDateTime::currentSecsSinceEpoch();
    const std::string build = build_identity();

    // A launch that never drew is accounted for here, before this one starts.
    cube::graphics::record record = load_record(*settings_);
    record = cube::graphics::account_for_previous_launch(record, build, now, asked.setting);
    choice_ = cube::graphics::decide(asked, offered, record, build, now);

    // Down the chain to the first backend that can start here. The backend
    // being asked about is the pending one while it is asked, so a probe that
    // itself takes the process down (a broken driver) is a strike against it.
    // Not asked about the last: nothing is left to fall back to.
    std::size_t chosen = 0;
    while (chosen + 1 < choice_.chain.size()) {
        const std::string& key = choice_.chain[chosen];
        if (choice_.guarded) {
            store_record(*settings_, cube::graphics::launch_started(record, key));
        }
        if (!usable || usable(key)) {
            break;
        }
        choice_.notes.push_back("skipped '" + key + "': it cannot start on this machine");
        ++chosen;
    }
    choice_.chain.erase(choice_.chain.begin(),
                        choice_.chain.begin() + static_cast<std::ptrdiff_t>(chosen));

    for (const std::string& note : choice_.notes) {
        qWarning("graphics: %s", note.c_str());
    }

    if (!choice_.chain.empty()) {
        backend_ = choice_.chain.front();
        if (backend_ == cube::graphics::software) {
            prefer_software_rasteriser();
            if (const auto api = api_for(software_api)) {
                QQuickWindow::setGraphicsApi(*api);
            }
        } else if (const auto api = api_for(backend_)) {
            QQuickWindow::setGraphicsApi(*api);
        }
        qInfo("graphics: %s (%s)", backend_.c_str(),
              std::string(cube::graphics::source_name(choice_.from)).c_str());
    }

    if (choice_.guarded && !backend_.empty()) {
        record = cube::graphics::launch_started(record, backend_);
    }
    // Written even when nothing is started, so the strikes and the lapsed
    // failures worked out above are kept.
    store_record(*settings_, record);
}

void Launch::watch(QQuickWindow& window) {
    if (!choice_.guarded || backend_.empty()) {
        return;
    }
    connect(
        &window, &QQuickWindow::frameSwapped, this, [this]() { confirm(); },
        Qt::SingleShotConnection);
    connect(&window, &QQuickWindow::sceneGraphError, this,
            [this](QQuickWindow::SceneGraphError, const QString& message) { failed(message); });
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        if (confirmed_ || relaunching_) {
            return;
        }
        store_record(*settings_, cube::graphics::launch_ended(load_record(*settings_), backend_));
    });
}

void Launch::confirm() {
    if (confirmed_ || relaunching_) {
        return;
    }
    confirmed_ = true;
    store_record(*settings_, cube::graphics::launch_succeeded(load_record(*settings_), backend_));
}

void Launch::failed(const QString& reason) {
    // After the first frame an error is the scene graph's business (a device
    // lost mid-run is not "this API cannot start"); the application does not
    // restart itself over it.
    if (confirmed_ || relaunching_) {
        return;
    }
    relaunching_ = true;
    qWarning("graphics: %s could not create the window (%s)", backend_.c_str(), qPrintable(reason));

    store_record(*settings_,
                 cube::graphics::launch_failed(load_record(*settings_), backend_, build_identity(),
                                               QDateTime::currentSecsSinceEpoch(), setting_));

    const std::vector<std::string> arguments =
        cube::graphics::relaunch_arguments(argv_tail_, skipped_, backend_);
    if (cube::platform::desktop_shell::start_again(arguments)) {
        QCoreApplication::exit(0);
    } else {
        qCritical("graphics: could not start the application again");
    }
}

}  // namespace cube::app::graphics