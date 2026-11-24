// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
// A button that is switched off, and whether its label can be read.
//
// AppButton.qml dims the whole button (fill, label and edge) to
// Theme.disabledOpacity when it is disabled, and does nothing else to it. This
// loads the real button in each theme, in each of its three looks, reads from
// the items what they are drawn in, and works out what the scene graph shows:
// each item is blended over what is under it at the product of the opacities up
// its parents, one after another (the fill over the page, the label over the
// fill), which is not the same as dimming the finished button as one picture.
//
// The design holds a disabled label to 3:1 although WCAG exempts it
// (docs/design/system.md, "Measured"). That is held here for the secondary
// button and chip, the label of which is ink on the page. A filled button's label
// (the chosen chip, a primary button) and a ghost button's (the brand red) cannot
// reach it by dimming, because the colours they start from are only 3.8:1 and
// 4.2:1 and both fade towards the page together; what is held for them is that
// they stay as legible as they are now. It needs neither a GPU nor a display.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "contrast.hpp"
#include "qml_test_app.hpp"

#include <QColor>
#include <QObject>
#include <QQmlComponent>
#include <QQuickItem>
#include <QString>
#include <QVariant>

#include <memory>
#include <string>

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

constexpr const char* kLabel = "Slow";

[[nodiscard]] std::unique_ptr<QQuickItem> make_button(const QString& variant, bool enabled) {
    QQmlComponent component(&cube::test::qml_engine());
    component.loadFromModule("Cubeulator", "AppButton");
    if (component.isError()) {
        FAIL(component.errorString().toStdString());
    }
    std::unique_ptr<QQuickItem> button(
        qobject_cast<QQuickItem*>(component.createWithInitialProperties(
            {{"text", QString::fromUtf8(kLabel)}, {"variant", variant}, {"enabled", enabled}})));
    REQUIRE(button != nullptr);
    return button;
}

[[nodiscard]] QQuickItem* find_item(QQuickItem* from, const char* class_name,
                                    const QString& text = QString()) {
    for (QQuickItem* child : from->childItems()) {
        if (child->inherits(class_name) &&
            (text.isEmpty() || child->property("text").toString() == text)) {
            return child;
        }
        if (QQuickItem* deeper = find_item(child, class_name, text)) {
            return deeper;
        }
    }
    return nullptr;
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

// `drawn` as the scene graph blends it over `below`, an item at `opacity`.
[[nodiscard]] QColor shown(QColor drawn, double opacity, const QColor& below) {
    drawn.setAlphaF(static_cast<float>(cube::test::alpha_of(drawn) * opacity));
    return cube::test::over(drawn, below);
}

struct Look {
    QColor fill;   // what the button's own fill shows, over the page
    QColor label;  // what its label shows, over that
    double label_on_fill = 1.0;
    double label_on_page = 1.0;
    double fill_on_page = 1.0;
};

[[nodiscard]] Look look_of(const QString& variant, bool enabled, const QColor& page) {
    const std::unique_ptr<QQuickItem> button = make_button(variant, enabled);
    // The fill is the button's own first Rectangle; the focus ring's are inside
    // an Item of their own and hidden.
    QQuickItem* fill = nullptr;
    for (QQuickItem* child : button->childItems()) {
        if (child->inherits("QQuickRectangle")) {
            fill = child;
            break;
        }
    }
    QQuickItem* label = find_item(button.get(), "QQuickText", kLabel);
    REQUIRE(fill != nullptr);
    REQUIRE(label != nullptr);

    Look look;
    look.fill = shown(fill->property("color").value<QColor>(), effective_opacity(fill), page);
    look.label =
        shown(label->property("color").value<QColor>(), effective_opacity(label), look.fill);
    look.label_on_fill = cube::test::contrast_ratio(look.label, look.fill);
    look.label_on_page = cube::test::contrast_ratio(look.label, page);
    look.fill_on_page = cube::test::contrast_ratio(look.fill, page);
    return look;
}

}  // namespace

TEST_CASE("a switched-off button is dimmed to the one opacity, and no further",
          "[qml][button][theme]") {
    const double off = singleton("Theme").property("disabledOpacity").toDouble();
    for (const char* variant : {"primary", "secondary", "ghost"}) {
        CAPTURE(variant);
        CHECK(effective_opacity(make_button(variant, true).get()) == Catch::Approx(1.0));
        CHECK(effective_opacity(make_button(variant, false).get()) == Catch::Approx(off));
    }
    // It has to look switched off: a button that is nearly whole is read as one that
    // is on, and pressing it does nothing. Past 0.7 it does not look it.
    CHECK(off < 0.7);
}

TEST_CASE("the label of a switched-off secondary button or chip is 3:1 on what it sits on",
          "[qml][button][theme]") {
    const std::string mode = GENERATE(as<std::string>{}, "light", "dark");
    const bool on_a_pane = GENERATE(false, true);
    CAPTURE(mode, on_a_pane);

    const ThemeMode theme(QString::fromStdString(mode));
    const QColor page =
        singleton("Theme").property(on_a_pane ? "surface" : "paper").value<QColor>();

    const Look off = look_of("secondary", false, page);
    const Look on = look_of("secondary", true, page);
    CAPTURE(page.name().toStdString(), off.label.name().toStdString(),
            on.label.name().toStdString(), off.label_on_page, on.label_on_page);
    CHECK(off.label_on_page >= cube::test::kLargeOrMutedContrast);
    // Switched off still reads as switched off: well below the same label whole.
    CHECK(off.label_on_page < on.label_on_page - 3.0);
}

TEST_CASE("a switched-off filled or ghost button is no harder to read than it is now",
          "[qml][button][theme]") {
    // These cannot be held to 3:1 by dimming (see the head of this file), so what
    // this holds is a floor, at what 0.6 gives (1.70 and 1.74 label on fill, 2.39 and
    // 2.20 fill and ghost label on the page, light and dark). Lowering the opacity
    // lowers them; if the floor has to go, the design's own look for a disabled
    // primary button (docs/design/system.md, "Button": the surface fill, the
    // divider edge and the disabled ink) is the way to 3:1, and the floor goes with it.
    const std::string mode = GENERATE(as<std::string>{}, "light", "dark");
    CAPTURE(mode);

    const ThemeMode theme(QString::fromStdString(mode));
    const QColor page = singleton("Theme").property("paper").value<QColor>();

    const Look chosen = look_of("primary", false, page);
    CAPTURE(chosen.label_on_fill, chosen.fill_on_page);
    CHECK(chosen.label_on_fill >= 1.65);
    CHECK(chosen.fill_on_page >= 2.15);

    const Look ghost = look_of("ghost", false, page);
    CAPTURE(ghost.label_on_page);
    CHECK(ghost.label_on_page >= 2.15);

    // Whole, each is what the design says of it, or near enough to be legible.
    CHECK(look_of("primary", true, page).label_on_fill >= 3.5);
    CHECK(look_of("ghost", true, page).label_on_page >= 3.5);
}
