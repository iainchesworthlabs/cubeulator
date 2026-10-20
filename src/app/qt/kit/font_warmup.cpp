#include "font_warmup.hpp"

#include <cube/profiling.hpp>

#include <QFont>
#include <QFontMetricsF>
#include <QString>

#include <utility>

namespace cube::app::kit {

namespace {

// The weights the screens set text in (grep font.weight in src/app/qt): the
// first lookup of a family is what costs, but each weight is an engine of its
// own, and an engine made here is one the GUI thread does not make.
constexpr QFont::Weight kWeights[] = {QFont::Normal, QFont::Medium, QFont::DemiBold, QFont::Bold,
                                      QFont::ExtraBold};

}  // namespace

QStringList themeFamilies() {
    return {QStringLiteral("Archivo"), QStringLiteral("IBM Plex Mono")};
}

void warmFonts(const QStringList& families) {
    CUBE_ZONE_SCOPED_N("app.font_warmup");
    for (const QString& family : families) {
        for (const QFont::Weight weight : kWeights) {
            QFont font(family);
            font.setPixelSize(14);
            font.setWeight(weight);
            // Measuring makes the engine; asking for the family alone is lazy.
            (void)QFontMetricsF(font).horizontalAdvance(QStringLiteral("Ag"));
        }
    }
}

FontWarmup::FontWarmup(QStringList families, bool enabled) {
    if (!enabled) {
        return;
    }
    worker_ = std::thread([families = std::move(families)] {
        CUBE_THREAD_NAME("font-warmup");
        warmFonts(families);
    });
}

FontWarmup::~FontWarmup() {
    wait();
}

void FontWarmup::wait() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

}  // namespace cube::app::kit
