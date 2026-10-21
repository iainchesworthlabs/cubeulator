// The About screen and the Licences screen under it (AboutScreen.qml,
// LicencesScreen.qml; docs/design/screens/about.md and licences.md), loaded for
// real behind the app's own controllers, with the notices the build generated.
//
// What is held here is what the screens are for: that every component that
// ships, the bundled fonts among them, can be found in the list and opened to its
// licence text, in full, as its authors wrote it.
#include <catch2/catch_test_macros.hpp>

#include "kit/bundled_fonts.hpp"
#include "qml_test_app.hpp"

#include <QMetaObject>
#include <QObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <memory>

namespace {

[[nodiscard]] QObject& app_state() {
    auto* state = cube::test::qml_engine().singletonInstance<QObject*>("Cubeulator", "AppState");
    REQUIRE(state != nullptr);
    return *state;
}

// A screen of the app's own, loaded for real behind the app's controllers.
class Loaded {
public:
    explicit Loaded(const char* type) {
        QQmlComponent component(&cube::test::qml_engine());
        component.loadFromModule("Cubeulator", type);
        if (component.isError()) {
            FAIL(component.errorString().toStdString());
        }
        item_.reset(qobject_cast<QQuickItem*>(component.create()));
        if (item_ == nullptr) {
            FAIL(component.errorString().toStdString());
        }
        item_->setWidth(900);
        item_->setHeight(640);
    }

    [[nodiscard]] QObject* find(const char* name) const {
        if (auto* found = item_->findChild<QObject*>(QString::fromLatin1(name))) {
            return found;
        }
        return find_visual(item_.get(), QString::fromLatin1(name));
    }
    [[nodiscard]] QObject& require(const char* name) const {
        QObject* found = find(name);
        INFO(name);
        REQUIRE(found != nullptr);
        return *found;
    }
    [[nodiscard]] QVariant get(const char* name) const { return item_->property(name); }
    void call(const char* method) const { REQUIRE(QMetaObject::invokeMethod(item_.get(), method)); }
    void open(int index) const {
        REQUIRE(QMetaObject::invokeMethod(item_.get(), "open", Q_ARG(QVariant, QVariant(index))));
    }
    void click(const char* name) const {
        REQUIRE(QMetaObject::invokeMethod(&require(name), "clicked"));
    }
    [[nodiscard]] bool visible(const char* name) const {
        return require(name).property("visible").toBool();
    }

private:
    static QQuickItem* find_visual(QQuickItem* item, const QString& name) {
        if (item->objectName() == name) {
            return item;
        }
        for (QQuickItem* child : item->childItems()) {
            if (QQuickItem* found = find_visual(child, name)) {
                return found;
            }
        }
        return nullptr;
    }

    std::unique_ptr<QQuickItem> item_;
};

// Puts the app back where it found it, and the notices where they were.
struct Tidy {
    Tidy() = default;
    Tidy(const Tidy&) = delete;
    Tidy& operator=(const Tidy&) = delete;
    Tidy(Tidy&&) = delete;
    Tidy& operator=(Tidy&&) = delete;
    ~Tidy() {
        app_state().setProperty("screen", QStringLiteral("launch"));
        cube::test::app_controllers().about.setNoticesPath(
            cube::app::AboutInfo::defaultNoticesPath());
    }
};

}  // namespace

TEST_CASE("This build's notices are there for the Licences screen to read", "[licences][about]") {
    // The screen has a state for notices that are missing, and it should never be
    // seen: a build that cannot hand its notices over breaks the licences it is
    // built on. The build embeds them; the tests do too.
    const auto& about = cube::test::app_controllers().about;
    REQUIRE(about.noticesAvailable());
    CHECK_FALSE(about.components().isEmpty());
    CHECK(about.sourceUrl().startsWith(QStringLiteral("https://")));
}

TEST_CASE("Every bundled font can be found in the Licences list and opened to its licence",
          "[licences][fonts]") {
    const Tidy tidy;
    const QStringList families = cube::app::kit::installBundledFonts();
    REQUIRE_FALSE(families.isEmpty());

    const Loaded screen("LicencesScreen");
    const QVariantList entries = screen.get("entries").toList();
    REQUIRE(entries.size() > 1);

    for (const QString& family : families) {
        INFO("family: " << family.toStdString());
        int found = -1;
        for (int i = 0; i < entries.size(); ++i) {
            const QVariantMap entry = entries[i].toMap();
            // "Archivo (typeface)" is the entry for "Archivo", and for the legacy
            // names its weights register under ("Archivo ExtraBold").
            if (entry.value("kind").toString() == QStringLiteral("font") &&
                family.startsWith(
                    entry.value("name").toString().section(QStringLiteral(" ("), 0, 0))) {
                found = i;
            }
        }
        REQUIRE(found > 0);  // 0 is Cubeulator's own row

        // It is in the list (its row is made when the list is on a window:
        // tests/system/licences_test.cpp finds it there), and opens to the
        // licence, in full, with its copyright line.
        screen.open(found);
        CHECK(screen.get("reading").toBool());
        CHECK(screen.visible("licences.detail"));
        const QString text = screen.require("licences.text").property("text").toString();
        const QString copyright = entries[found].toMap().value("copyright").toString();
        CHECK_FALSE(copyright.isEmpty());
        CHECK(text.contains(copyright));
        CHECK(text.contains(QStringLiteral("SIL OPEN FONT LICENSE Version 1.1")));
        CHECK(text.contains(QStringLiteral("DISCLAIMER")));
        screen.call("goBack");
        CHECK_FALSE(screen.get("reading").toBool());
        CHECK(screen.visible("licences.list"));
    }
}

TEST_CASE("Every component in the list opens to a notice", "[licences]") {
    const Tidy tidy;
    const Loaded screen("LicencesScreen");
    const QVariantList entries = screen.get("entries").toList();
    REQUIRE(entries.size() > 1);

    // Cubeulator's own row first, then the components by name.
    CHECK(entries[0].toMap().value("own").toBool());
    CHECK(entries[0].toMap().value("licence").toString() == QStringLiteral("GPL-3.0-or-later"));
    QString previous;
    for (int i = 1; i < entries.size(); ++i) {
        const QVariantMap entry = entries[i].toMap();
        const QString name = entry.value("name").toString();
        INFO("component: " << name.toStdString());
        CHECK_FALSE(name.isEmpty());
        CHECK_FALSE(entry.value("licence").toString().isEmpty());
        CHECK_FALSE(entry.value("text").toString().trimmed().isEmpty());
        CHECK(QString::compare(previous, name, Qt::CaseInsensitive) <= 0);
        previous = name;
    }

    // Opening the first and the last shows their name and their text.
    for (const int index : {0, static_cast<int>(entries.size()) - 1}) {
        screen.open(index);
        const QVariantMap entry = entries[index].toMap();
        CHECK(screen.require("licences.text").property("text").toString() ==
              entry.value("text").toString());
        screen.call("goBack");
    }
}

TEST_CASE("Back leaves the notice for the list, and the list for About", "[licences]") {
    const Tidy tidy;
    app_state().setProperty("screen", QStringLiteral("licences"));
    const Loaded screen("LicencesScreen");

    screen.open(1);
    REQUIRE(screen.get("reading").toBool());
    screen.click("licences.back");
    CHECK_FALSE(screen.get("reading").toBool());
    CHECK(app_state().property("screen").toString() == QStringLiteral("licences"));

    screen.click("licences.back");
    CHECK(app_state().property("screen").toString() == QStringLiteral("about"));
}

TEST_CASE("A notice is left to right and can be selected", "[licences]") {
    const Tidy tidy;
    const Loaded screen("LicencesScreen");
    screen.open(1);
    const QObject& text = screen.require("licences.text");
    CHECK(text.property("readOnly").toBool());
    CHECK(text.property("selectByMouse").toBool());
    CHECK(text.property("effectiveHorizontalAlignment").toInt() == static_cast<int>(Qt::AlignLeft));
}

TEST_CASE("Notices that cannot be read are said so, with where they are published", "[licences]") {
    const Tidy tidy;
    cube::test::app_controllers().about.setNoticesPath(QStringLiteral("no-such-notices.json"));
    const Loaded screen("LicencesScreen");
    CHECK(screen.visible("licences.unavailable"));
    CHECK_FALSE(screen.visible("licences.list"));
    CHECK_FALSE(screen.visible("licences.detail"));
}

TEST_CASE("About leads to the notices and names the source", "[about]") {
    const Tidy tidy;
    app_state().setProperty("screen", QStringLiteral("about"));
    const Loaded screen("AboutScreen");

    // The words are the catalogues' (the screen tests that have them hold what
    // is on show); the data is the build's.
    CHECK(screen.require("about.source").property("value").toString() ==
          cube::test::app_controllers().about.sourceUrl());

    screen.click("about.licences");
    CHECK(app_state().property("screen").toString() == QStringLiteral("licences"));
    REQUIRE(QMetaObject::invokeMethod(&app_state(), "closeLicences"));
    CHECK(app_state().property("screen").toString() == QStringLiteral("about"));
}

TEST_CASE("About goes back to where it was opened from", "[about]") {
    const Tidy tidy;
    for (const char* from : {"settings", "launch", "history", "complete"}) {
        INFO("opened from " << from);
        app_state().setProperty("screen", QString::fromLatin1(from));
        REQUIRE(QMetaObject::invokeMethod(&app_state(), "openAbout"));
        CHECK(app_state().property("screen").toString() == QStringLiteral("about"));
        // Licences and back again does not lose the place it was opened from.
        REQUIRE(QMetaObject::invokeMethod(&app_state(), "openLicences"));
        REQUIRE(QMetaObject::invokeMethod(&app_state(), "closeLicences"));
        REQUIRE(QMetaObject::invokeMethod(&app_state(), "closeAbout"));
        CHECK(app_state().property("screen").toString() == QString::fromLatin1(from));
    }
}

TEST_CASE("About is not opened over a scan or a solve", "[about]") {
    const Tidy tidy;
    for (const char* from : {"scan", "solve"}) {
        INFO("on " << from);
        app_state().setProperty("screen", QString::fromLatin1(from));
        REQUIRE(QMetaObject::invokeMethod(&app_state(), "openAbout"));
        CHECK(app_state().property("screen").toString() == QString::fromLatin1(from));
    }
}
