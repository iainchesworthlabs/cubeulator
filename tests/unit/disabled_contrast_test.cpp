// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// A button that is switched off, and whether it can be read.
//
// AppButton.qml does not fade a button that is switched off. It draws it in a
// style of its own: an opaque Theme.surface plate, an edge in Theme.outline, the
// label (and any icon) in Theme.inkDisabled, and, for the chosen one of a set (a
// chip), a second ring inside the edge, so that which is chosen is told by a
// shape and not by colour alone. This loads the real button in each theme, in
// each look it has (the secondary button and chip, the chosen chip, the primary
// button, the ghost button, and each of them over the camera picture), reads from
// the items what they are drawn in, and holds that:
//
//   - nothing in it is faded: every part is at an opacity of 1;
//   - the plate is opaque, so the label is read against the plate and never
//     against what is behind the button, which over the camera is not known;
//   - the label is 3:1 on the plate (the design holds a disabled label to 3:1
//     although WCAG exempts it: docs/design/system.md, "Measured");
//   - the edge is 3:1 on the page and on a pane (not measured over the camera,
//     where the page is the picture);
//   - the chosen one has the ring and no other has it.
//
// The scan screen's own Start over, which really is switched off until a side is
// read, is checked the same way. It needs neither a GPU nor a display.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "contrast.hpp"
#include "qml_test_app.hpp"

#include <QColor>
#include <QMetaObject>
#include <QObject>
#include <QQmlComponent>
#include <QQuickItem>
#include <QString>
#include <QVariant>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

[[nodiscard]] QObject& singleton(const char* name) {
    QObject* found = cube::test::qml_engine().singletonInstance<QObject*>("Cubeulator", name);
    REQUIRE(found != nullptr);
    return *found;
}

// The persisted theme choice, set for the length of a case and put back: the
// store is the one the whole QML test run shares.
class ThemeMode {
public:
    explicit ThemeMode(const QString& mode)
        : settings_(singleton("SettingsState")),
          previous_(settings_.property("themeMode").toString()) {
        REQUIRE(settings_.setProperty("themeMode", mode));
    }
    ~ThemeMode() { settings_.setProperty("themeMode", previous_); }
    ThemeMode(const ThemeMode&) = delete;
    ThemeMode& operator=(const ThemeMode&) = delete;

private:
    QObject& settings_;
    QString previous_;
};

[[nodiscard]] QColor theme_colour(const char* name) {
    const QVariant value = singleton("Theme").property(name);
    INFO("Theme." << name);
    REQUIRE(value.isValid());
    const QColor colour = value.value<QColor>();
    REQUIRE(colour.isValid());
    return colour;
}

[[nodiscard]] std::string hex(const QColor& colour) {
    return colour.name(QColor::HexArgb).toStdString();
}

constexpr const char* kLabel = "Slow";

// A look of the button, as a screen asks for it.
struct Spec {
    const char* name;
    const char* variant;  // primary | secondary | ghost
    bool chosen;          // the one chosen of a set, as a chip row draws it
    bool over_camera;     // on the scan screen
    bool icons;           // a leading and a trailing icon
    bool block;
};

// Every look a button has. The chosen chip is the primary look, checkable and
// checked, as SettingsScreen's chip row and the scan screen's size row draw it.
constexpr Spec kLooks[] = {
    {"plain (secondary button or chip)", "secondary", false, false, false, false},
    {"chosen (chip)", "primary", true, false, false, false},
    {"primary", "primary", false, false, false, false},
    {"ghost", "ghost", false, false, false, false},
    {"ghost with its icons", "ghost", false, false, true, false},
    {"primary, whole width, with its icons", "primary", false, false, true, true},
    {"scan screen, secondary (Cancel, Start over)", "secondary", false, true, false, false},
    {"scan screen, chosen size", "primary", true, true, false, false},
    {"scan screen, primary (Check the colours)", "primary", false, true, false, false},
};

[[nodiscard]] std::unique_ptr<QQuickItem> make_button(const Spec& spec, bool enabled) {
    QQmlComponent component(&cube::test::qml_engine());
    component.loadFromModule("Cubeulator", "AppButton");
    if (component.isError()) {
        FAIL(component.errorString().toStdString());
    }
    QVariantMap properties{{"text", QString::fromUtf8(kLabel)},
                           {"variant", QString::fromUtf8(spec.variant)},
                           {"enabled", enabled},
                           {"checkable", spec.chosen},
                           {"checked", spec.chosen},
                           {"overCamera", spec.over_camera},
                           {"block", spec.block}};
    if (spec.icons) {
        properties.insert("leadingIcon", "chevron-left");
        properties.insert("trailingIcon", "chevron-right");
    }
    std::unique_ptr<QQuickItem> button(
        qobject_cast<QQuickItem*>(component.createWithInitialProperties(properties)));
    REQUIRE(button != nullptr);
    button->setWidth(200);
    button->setHeight(44);
    return button;
}

[[nodiscard]] bool is_app_button(const QObject* object) {
    // The QML file's type name is a prefix of the class name ("AppButton_QMLTYPE_12").
    return std::strncmp(object->metaObject()->className(), "AppButton", 9) == 0;
}

void collect(QQuickItem* from, const char* class_name, std::vector<QQuickItem*>& into) {
    for (QQuickItem* child : from->childItems()) {
        if (child->inherits(class_name)) {
            into.push_back(child);
        }
        collect(child, class_name, into);
    }
}

[[nodiscard]] std::vector<QQuickItem*> all_app_buttons(QQuickItem* from) {
    std::vector<QQuickItem*> found;
    for (QQuickItem* child : from->childItems()) {
        if (is_app_button(child)) {
            found.push_back(child);
        }
        const std::vector<QQuickItem*> deeper = all_app_buttons(child);
        found.insert(found.end(), deeper.begin(), deeper.end());
    }
    return found;
}

// How much of what is behind an item shows through it and every item above it:
// the product of their opacities, its own included.
[[nodiscard]] double effective_opacity(const QQuickItem* item) {
    double seen = 1.0;
    for (; item != nullptr; item = item->parentItem()) {
        seen *= item->opacity();
    }
    return seen;
}

[[nodiscard]] QColor colour_of(const QQuickItem* item) {
    return item->property("color").value<QColor>();
}

[[nodiscard]] QObject* pen_of(const QQuickItem* rectangle) {
    return qvariant_cast<QObject*>(rectangle->property("border"));
}

// What a button is drawn in, read from its items.
struct Look {
    QQuickItem* face = nullptr;  // the plate
    QQuickItem* ring = nullptr;  // the ring inside the edge of the chosen one, if it has one
    QQuickItem* label = nullptr;
    std::vector<QQuickItem*> icons;

    [[nodiscard]] QColor fill() const { return colour_of(face); }
    [[nodiscard]] QColor edge() const { return pen_of(face)->property("color").value<QColor>(); }
    [[nodiscard]] double edge_width() const { return pen_of(face)->property("width").toDouble(); }
    // A button with no ring at all has none of either, and reads as it is.
    [[nodiscard]] QColor ring_colour() const {
        return ring == nullptr ? QColor() : pen_of(ring)->property("color").value<QColor>();
    }
    [[nodiscard]] double ring_width() const {
        return ring == nullptr ? 0.0 : pen_of(ring)->property("width").toDouble();
    }
    [[nodiscard]] bool ring_shown() const { return ring != nullptr && ring->isVisible(); }
    // Every part that draws something: nothing is faded if each is at 1.
    [[nodiscard]] std::vector<const QQuickItem*> drawn_parts() const {
        std::vector<const QQuickItem*> parts{face, label};
        if (ring_shown()) {
            parts.push_back(ring);
        }
        parts.insert(parts.end(), icons.begin(), icons.end());
        return parts;
    }
};

[[nodiscard]] Look look_of(QQuickItem* button) {
    Look look;
    for (QQuickItem* child : button->childItems()) {
        if (!child->inherits("QQuickRectangle")) {
            continue;
        }
        if (child->objectName() == "appButton.chosenRing") {
            look.ring = child;
        } else if (look.face == nullptr) {
            // The button's own plate is its first Rectangle; the focus ring's are
            // inside an Item of their own and hidden.
            look.face = child;
        }
    }
    // The label is the Text that says what the button says.
    std::vector<QQuickItem*> texts;
    collect(button, "QQuickText", texts);
    for (QQuickItem* text : texts) {
        if (text->property("text").toString() == button->property("text").toString()) {
            look.label = text;
        }
    }
    std::vector<QQuickItem*> canvases;
    collect(button, "QQuickCanvasItem", canvases);
    for (QQuickItem* icon : canvases) {
        if (icon->isVisible() && !icon->property("name").toString().isEmpty()) {
            look.icons.push_back(icon);
        }
    }
    REQUIRE(look.face != nullptr);
    REQUIRE(look.label != nullptr);
    return look;
}

// `drawn` as the scene graph blends it over `below`, an item at `opacity`.
[[nodiscard]] QColor shown(QColor drawn, double opacity, const QColor& below) {
    drawn.setAlphaF(static_cast<float>(cube::test::alpha_of(drawn) * opacity));
    return cube::test::over(drawn, below);
}

// What a switched-off button must be, said once and held for every one of them.
// `pages` are what it may sit on and be told from (the page and a pane; none
// over the camera, where the page is a picture nobody can say anything of).
void check_switched_off(const Look& look, bool chosen, const std::vector<QColor>& pages) {
    const QColor surface = theme_colour("surface");
    const QColor ink_disabled = theme_colour("inkDisabled");
    const QColor outline = theme_colour("outline");

    // Nothing is faded.
    for (const QQuickItem* part : look.drawn_parts()) {
        CAPTURE(part->metaObject()->className());
        CHECK(effective_opacity(part) == Catch::Approx(1.0));
    }

    // An opaque plate: what is behind the button does not reach the label.
    CAPTURE(hex(look.fill()), hex(surface));
    CHECK(hex(look.fill()) == hex(surface));
    CHECK(cube::test::is_opaque(look.fill()));

    // The label, and any icon, is the disabled ink and is 3:1 on the plate.
    CAPTURE(hex(colour_of(look.label)), hex(ink_disabled));
    CHECK(hex(colour_of(look.label)) == hex(ink_disabled));
    const double label_on_plate = cube::test::contrast_ratio(colour_of(look.label), look.fill());
    CAPTURE(label_on_plate);
    CHECK(label_on_plate >= cube::test::kLargeOrMutedContrast);
    for (const QQuickItem* icon : look.icons) {
        CHECK(hex(colour_of(icon)) == hex(ink_disabled));
        CHECK(cube::test::contrast_ratio(colour_of(icon), look.fill()) >=
              cube::test::kLargeOrMutedContrast);
    }

    // A plain edge, which the plate is told from the page by.
    CAPTURE(hex(look.edge()), hex(outline), look.edge_width());
    CHECK(look.edge_width() >= 1.0);
    CHECK(hex(look.edge()) == hex(outline));
    CHECK(cube::test::is_opaque(look.edge()));
    for (const QColor& page : pages) {
        CAPTURE(hex(page));
        CHECK(cube::test::contrast_ratio(look.edge(), page) >= cube::test::kLargeOrMutedContrast);
    }

    // Which is chosen is a shape: the second ring is there for the chosen one and
    // for no other, and is itself 3:1 on the plate.
    CHECK(look.ring_shown() == chosen);
    if (chosen) {
        CHECK(hex(look.ring_colour()) == hex(outline));
        CHECK(look.ring_width() >= 2.0);
        CHECK(cube::test::contrast_ratio(look.ring_colour(), look.fill()) >=
              cube::test::kLargeOrMutedContrast);
    }
}

}  // namespace

TEST_CASE("a switched-off button is an opaque grey plate, 3:1 label and edge, in every look",
          "[qml][button][theme]") {
    for (const char* mode : {"light", "dark"}) {
        const ThemeMode theme{QString::fromUtf8(mode)};
        const std::vector<QColor> pages{theme_colour("paper"), theme_colour("surface")};
        for (const Spec& spec : kLooks) {
            INFO(mode << ", " << spec.name);
            const std::unique_ptr<QQuickItem> button = make_button(spec, false);
            REQUIRE_FALSE(button->isEnabled());
            const Look look = look_of(button.get());
            check_switched_off(look, spec.chosen, spec.over_camera ? std::vector<QColor>{} : pages);
            CHECK(effective_opacity(button.get()) == Catch::Approx(1.0));
        }
    }
}

TEST_CASE("a switched-off button looks switched off, and one that is on is whole",
          "[qml][button][theme]") {
    for (const char* mode : {"light", "dark"}) {
        const ThemeMode theme{QString::fromUtf8(mode)};
        const QColor page = theme_colour("paper");
        const QColor ink_disabled = theme_colour("inkDisabled");
        for (const Spec& spec : kLooks) {
            INFO(mode << ", " << spec.name);
            const std::unique_ptr<QQuickItem> on_button = make_button(spec, true);
            const std::unique_ptr<QQuickItem> off_button = make_button(spec, false);
            const Look on = look_of(on_button.get());
            const Look off = look_of(off_button.get());

            // On, it has none of the switched-off look: its own label, no ring, whole.
            CHECK(hex(colour_of(on.label)) != hex(ink_disabled));
            CHECK_FALSE(on.ring_shown());
            CHECK(effective_opacity(on_button.get()) == Catch::Approx(1.0));
            // And off differs from on by more than a shade: the label and the plate.
            CHECK(hex(colour_of(off.label)) != hex(colour_of(on.label)));
            CHECK(hex(off.edge()) != hex(on.edge()));

            // On, it is as legible as it was: the primary's paper on the brand red
            // and the ghost's red on the page are 3.5:1 or better, the secondary's
            // ink is 4.5:1.
            const QColor on_fill = shown(on.fill(), effective_opacity(on.face), page);
            const QColor on_label =
                shown(colour_of(on.label), effective_opacity(on.label), on_fill);
            const double ratio = cube::test::contrast_ratio(on_label, on_fill);
            CAPTURE(ratio);
            CHECK(ratio >=
                  (std::string(spec.variant) == "secondary" ? cube::test::kTextContrast : 3.5));
        }
    }
}

TEST_CASE("the scan screen's Start over is the same plate, whatever the camera shows",
          "[qml][button][theme][direct-scan]") {
    for (const char* mode : {"light", "dark"}) {
        const ThemeMode theme{QString::fromUtf8(mode)};
        INFO(mode);

        // No side is read, so Start over is switched off.
        cube::test::app_controllers().directScan.restart();
        QQmlComponent component(&cube::test::qml_engine());
        component.loadFromModule("Cubeulator", "DirectScanScreen");
        if (component.isError()) {
            FAIL(component.errorString().toStdString());
        }
        const std::unique_ptr<QQuickItem> screen(qobject_cast<QQuickItem*>(component.create()));
        REQUIRE(screen != nullptr);
        screen->setWidth(900);
        screen->setHeight(640);

        int checked = 0;
        for (QQuickItem* button : all_app_buttons(screen.get())) {
            if (button->isEnabled() || !button->property("overCamera").toBool()) {
                continue;
            }
            INFO("a switched-off button on the scan screen: "
                 << button->property("text").toString().toStdString());
            // Over the camera there is no page to tell its edge from, but the plate
            // and the label are the same, and are all that it takes to read it.
            check_switched_off(look_of(button), button->property("checked").toBool(), {});
            ++checked;
        }
        CHECK(checked >= 1);
    }
}
