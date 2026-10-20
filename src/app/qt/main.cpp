#include "app_runtime.hpp"
#include "controllers/scan_reading.hpp"
#include "desktop/desktop_integration.hpp"
#include "interfaces/crash_handler.hpp"
#include "interfaces/platform_info.hpp"
#include "kit/bundled_fonts.hpp"
#include "kit/font_warmup.hpp"
#include "qsg_log_bridge.hpp"
#include "render_profiler.hpp"

#include <cube/nxn/cube.hpp>
#include <cube/profile/session.hpp>
#include <cube/profiling.hpp>
#include <cube/solver/solver.hpp>
#include <cube/version.hpp>

#include <QGuiApplication>
#include <QMetaObject>
#include <QQuickWindow>
#include <QSettings>
#include <QThreadPool>

#include <algorithm>
#include <memory>

namespace {

// The graphics API a QQuickWindow uses is fixed for the process's lifetime
// (QQuickWindow::setGraphicsApi() only has an effect before the first
// QGuiApplication/QQuickWindow exists), so the "stats for nerds" backend
// picker in SettingsScreen can only persist a preference for next launch,
// not hot-swap it. This reads that preference from the same QSettings store
// the Settings QML type (QtCore's, in SettingsState.qml) writes to (same
// organization/application name), applied here before anything else runs.
void applyPersistedGraphicsBackend() {
    QCoreApplication::setOrganizationName("Cubeulator");
    QCoreApplication::setApplicationName("Cubeulator");

    const QSettings settings;
    const auto backend = settings.value("rendering/graphicsBackend", "auto").toString();
    if (backend == "d3d11") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    } else if (backend == "d3d12") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);
    } else if (backend == "vulkan") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    } else if (backend == "opengl") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    } else if (backend == "metal") {
        QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
    }
    // "auto" (or an unrecognised value): leave Qt's own per-platform default.
    // Values are only ever written by SettingsState.qml's
    // availableGraphicsBackends, which already filters to what's valid on
    // the current platform (Direct3D: Windows only; Metal: macOS/iOS only;
    // Vulkan/OpenGL: wherever Qt ships a backend for them) - no extra
    // per-platform guarding needed here.
}

// What switched the profiler on, for the one line the application logs.
const char* profilingSwitchName(cube::profile::switch_source source) {
    switch (source) {
        case cube::profile::switch_source::command_line:
            return "--profile";
        case cube::profile::switch_source::environment:
            return "CUBEULATOR_PROFILE";
        case cube::profile::switch_source::build_default:
            break;
    }
    return "this build profiles by default";
}

// Holds the profiler's shutdown back until the pool threads that may be inside
// a zone have finished (a solve is one): Tracy's client must not be torn down
// while a thread can still write to it. It must be destroyed while the
// QGuiApplication still exists (the global pool goes with it) and before the
// profiling Session, so main() declares it right after the application.
class ProfilerQuiesce {
public:
    explicit ProfilerQuiesce(const cube::profile::Session& session) : session_(session) {}
    ~ProfilerQuiesce() {
        if (session_.running()) {
            constexpr int kWaitMs = 5000;
            QThreadPool::globalInstance()->waitForDone(kWaitMs);
        }
    }
    ProfilerQuiesce(const ProfilerQuiesce&) = delete;
    ProfilerQuiesce& operator=(const ProfilerQuiesce&) = delete;
    ProfilerQuiesce(ProfilerQuiesce&&) = delete;
    ProfilerQuiesce& operator=(ProfilerQuiesce&&) = delete;

private:
    const cube::profile::Session& session_;
};

}  // namespace

int main(int argc, char* argv[]) {
    // The profiler comes first of all, so every thread the application starts
    // can name itself to it and the whole run is on the timeline. The Tracy
    // client linked into this binary is dormant: nothing listens and a zone is
    // one atomic load until this says otherwise - a Debug build by default, any
    // other build when CUBEULATOR_PROFILE is set or --profile is passed (no
    // recompile; docs/profiling.md). Declared first so it is destroyed last,
    // after every worker has stopped.
    const QByteArray profile_environment = qgetenv(cube::profile::environment_variable);
    const auto profile_arguments = cube::profile::arguments_of(argc, argv);
    const cube::profile::Session profiling(cube::profile::decide_startup(
        cube::profile::build_default_on(),
        profile_environment.isNull() ? nullptr : profile_environment.constData(),
        profile_arguments));
    if (profiling.running()) {
        cube::app::installQsgLogBridge();
        qInfo("Tracy profiler started (%s): connect a Tracy 0.14 profiler to 127.0.0.1:%s",
              profilingSwitchName(profiling.decision().source),
              qPrintable(qEnvironmentVariable("TRACY_PORT", QStringLiteral("8086"))));
    }

    // Installed before anything else so a fault during startup is captured
    // too. What it installs, if anything, is the platform's own business
    // (interfaces/crash_handler.hpp).
    cube::platform::crash_handler::install();

    applyPersistedGraphicsBackend();

    QGuiApplication app(argc, argv);

    // Destroyed before the application (and its global thread pool) and after
    // the runtime that owns the workers: see ProfilerQuiesce.
    const ProfilerQuiesce profiling_quiesce(profiling);

    // The fonts compiled into the app (:/fonts) are registered before any QML
    // asks for a family. A build that has none is unaffected.
    cube::app::kit::installBundledFonts();

    // Qt's font database is asked for the interface's families on a thread of
    // its own, while the engine and the screens are built; the first text on
    // the GUI thread would otherwise wait for that lookup (kit/font_warmup.hpp).
    // After the bundled fonts, so that it looks for what is there to be found.
    // Destroyed before the application and the profiling session it reports to.
    const cube::app::kit::FontWarmup font_warmup(
        cube::app::kit::themeFamilies(),
        cube::platform::platform_info::current().warm_fonts_off_thread);

    // Tracy names the GUI thread and carries the build's identity into every
    // trace, so a capture says which commit it measured (cube/profiling.hpp).
    CUBE_THREAD_NAME("main");
    CUBE_APP_INFO(cube::version_details());

    // The "startup" section brackets everything up to the first window: the
    // GUI can filter a capture to it, and it is where app.i18n_install and
    // app.load_qml spend their time.
    const std::uint32_t startup_section = CUBE_SECTION_ENTER("startup");

    // E4a - the process's identity (AppUserModelID, desktop file name, window
    // icon) and the remembered window geometry: what only the real application
    // does with its window. Declared before the runtime so that it outlives
    // the engine. The singletons DesktopShell.qml reads are the runtime's.
    cube::app::DesktopIntegration desktop(app);

    // The controllers (cube::app::AppControllers), their registration as QML
    // singletons and the engine that loads Main.qml are one AppRuntime, which
    // tests/system boots the same way.
    cube::app::AppRuntime runtime;
    desktop.prepare(runtime.engine());  // E4a: the window's initial geometry
    if (!runtime.load()) {
        return -1;
    }
    desktop.attach(runtime.engine());  // E4a: start remembering it
    SolveController& solveController = runtime.solve();
    QQmlApplicationEngine& engine = runtime.engine();

    CUBE_SECTION_LEAVE(startup_section);

    // Tracy's "UI" frame set, the CPU and GPU zones around each Qt Quick
    // presentation and the frame-time plot all hang off RenderProfiler,
    // whose handlers run on the scene graph's render thread (direct
    // connections, not queued: the marks must timestamp the swap itself).
    // The unnamed frame set is the vision pass and "Capture" the camera -
    // three unrelated cadences, three sets.
    if (QQuickWindow* window = runtime.window(); window != nullptr && cube::profiling::active()) {
        cube::app::RenderProfiler::install(*window);
    }

    // CUBE_SOLVE_DEMO=<number>: open on the solve screen with a cube
    // scrambled from that seed, no camera and no scan. For working on the
    // solve screen itself - trying how a move is shown does not need a real
    // cube read first. CUBE_SOLVE_DEMO_MOVE=<n> opens it n moves in, and
    // CUBE_SOLVE_DEMO_SIZE=<n> makes it a cube of that side length, 2 to 9
    // (3 unless said): the way to see a bigger cube without one in hand.
    bool demo = false;
    const unsigned demo_seed = qEnvironmentVariable("CUBE_SOLVE_DEMO").toUInt(&demo);
    if (demo) {
        const int moves_in = qEnvironmentVariable("CUBE_SOLVE_DEMO_MOVE").toInt();
        const int demo_size = std::clamp(qEnvironmentVariable("CUBE_SOLVE_DEMO_SIZE", "3").toInt(),
                                         cube::nxn::kMinSize, cube::nxn::kMaxSize);
        if (demo_size == 3) {
            solveController.solveCube(cube::solver::random_scramble(demo_seed));
        } else {
            solveController.solveCube(cube::nxn::random_scramble(
                {.size = demo_size, .seed = demo_seed, .moves = 40 + (8 * demo_size)}));
        }
        QObject* app_state = engine.singletonInstance<QObject*>("Cubeulator", "AppState");
        app_state->setProperty("solveCamera", false);
        auto show_when_solved = std::make_shared<QMetaObject::Connection>();
        *show_when_solved = QObject::connect(
            &solveController, &SolveController::stateChanged, app_state,
            [&solveController, app_state, show_when_solved, moves_in]() {
                if (solveController.isSolving()) {
                    return;
                }
                QObject::disconnect(*show_when_solved);
                QStringList solution;
                for (const QVariant& move : solveController.moves()) {
                    solution.append(move.toMap().value(QStringLiteral("notation")).toString());
                }
                qInfo("CUBE_SOLVE_DEMO: %s", qPrintable(solution.join(QLatin1Char(' '))));
                for (int i = 0; i < moves_in; ++i) {
                    solveController.nextMove();
                }
                app_state->setProperty("screen", QStringLiteral("solve"));
            });
    }

    // CUBE_REVIEW_DEMO=<number>: open on the screen where a misread scan is put
    // right, with a cube scrambled from that seed that the "camera" has misread (two
    // stickers swapped and marked as doubtful, and one more mark). For working on
    // that screen without scanning a cube badly on purpose. CUBE_REVIEW_DEMO_SIZE=<n>
    // makes it a cube of that side length, 2 to 9 (3 unless said);
    // CUBE_REVIEW_DEMO_TURNED=1 has the camera say it could not tell which way up
    // each side was held. The answer is printed.
    bool review = false;
    const unsigned review_seed = qEnvironmentVariable("CUBE_REVIEW_DEMO").toUInt(&review);
    if (review) {
        const int review_size =
            std::clamp(qEnvironmentVariable("CUBE_REVIEW_DEMO_SIZE", "3").toInt(),
                       cube::nxn::kMinSize, cube::nxn::kMaxSize);
        const bool turned = qEnvironmentVariable("CUBE_REVIEW_DEMO_TURNED") == QLatin1String("1");
        runtime.controllers().colours.loadReading(
            cube::app::demo_reading(review_seed, review_size, !turned));
        for (const auto& sticker : cube::app::demo_misread_stickers(review_seed, review_size)) {
            qInfo("CUBE_REVIEW_DEMO: side %c sticker %d should be colour %d", sticker.side,
                  sticker.index + 1, static_cast<int>(sticker.colour));
        }
        QObject* app_state = engine.singletonInstance<QObject*>("Cubeulator", "AppState");
        app_state->setProperty("solveCamera", false);
        app_state->setProperty("coloursFromScan", true);
        app_state->setProperty("screen", QStringLiteral("colours"));
    }

    return QGuiApplication::exec();
}
