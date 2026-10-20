#include "qsg_log_bridge.hpp"

#include <cube/profile/qsg_timing.hpp>
#include <cube/profiling.hpp>

#include <QLoggingCategory>
#include <QtLogging>

#include <string_view>

namespace cube::app {

namespace {

// Qt's message handler is one function pointer for the process, so what it
// needs lives in one place, behind a function.
struct BridgeState {
    QtMessageHandler previous_handler = nullptr;
    // Whether the user asked for these lines themselves (QT_LOGGING_RULES), in
    // which case they still reach the previous handler and so the console.
    bool keep_on_console = false;
};

BridgeState& state() noexcept {
    static BridgeState bridge;
    return bridge;
}

void forward(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    const std::string_view category =
        context.category != nullptr ? std::string_view{context.category} : std::string_view{};
    if (!cube::profile::is_qsg_timing_category(category)) {
        if (state().previous_handler != nullptr) {
            state().previous_handler(type, context, message);
        }
        return;
    }

    const QByteArray text = message.toUtf8();
    const cube::profile::qsg_line line = cube::profile::parse_qsg_timing(
        {.category = category,
         .text = std::string_view{text.constData(), static_cast<std::size_t>(text.size())}});
    for ([[maybe_unused]] const cube::profile::qsg_sample& sample : line.samples) {
        CUBE_PLOT(sample.plot, sample.value);
    }
    if (line.as_message) {
        const QByteArray tagged = QByteArray{context.category} + ": " + text;
        // A variable, not an expression: the macro would split a braced
        // initialiser at its comma.
        [[maybe_unused]] const std::string_view tagged_view{
            tagged.constData(), static_cast<std::size_t>(tagged.size())};
        CUBE_MESSAGE_DYNAMIC(tagged_view);
    }

    if (state().keep_on_console && state().previous_handler != nullptr) {
        state().previous_handler(type, context, message);
    }
}

}  // namespace

void installQsgLogBridge() {
    if (!cube::profiling::active() || state().previous_handler != nullptr) {
        return;
    }
    state().keep_on_console = qgetenv("QT_LOGGING_RULES").contains("qt.scenegraph");
    QLoggingCategory::setFilterRules(QStringLiteral("qt.scenegraph.time.*=true"));
    state().previous_handler = qInstallMessageHandler(forward);
}

}  // namespace cube::app
