#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace cube::app {

// What the About and Licences screens show (docs/design/screens/about.md and
// licences.md): the app's name, version, publisher and the address of its
// source, and the third-party notices the build generated and the app embeds as
// :/licences/third-party-notices.json (docs/licensing.md).
//
// The notices are read on first use, not at start-up: they are a few hundred
// kilobytes of JSON that nothing needs until a person opens Licences. Nothing
// here is written by hand; the list is whatever the generator found to ship in
// this build, so it cannot drift from it. Read-only.
class AboutInfo : public QObject {
    Q_OBJECT
    // Never translated.
    Q_PROPERTY(QString name READ name CONSTANT)
    // The build's own version string, the one the installer and a store show.
    Q_PROPERTY(QString version READ version CONSTANT)
    // The version as a screen shows it inside a sentence: the same number,
    // isolated left to right so that a right-to-left line cannot reorder it.
    Q_PROPERTY(QString displayVersion READ displayVersion CONSTANT)
    // Who made it, as the packages say it (CUBEULATOR_PUBLISHER, one line in
    // cmake/AppIdentity.cmake).
    Q_PROPERTY(QString publisher READ publisher CONSTANT)
    // Where the source is: the GPL obliges the app to say so.
    Q_PROPERTY(QString sourceUrl READ sourceUrl NOTIFY noticesChanged)
    // The licence the app itself is under, an SPDX identifier.
    Q_PROPERTY(QString licenceId READ licenceId NOTIFY noticesChanged)
    // False when the generated notices could not be read: the Licences screen
    // says so, and where the same notices are published.
    Q_PROPERTY(bool noticesAvailable READ noticesAvailable NOTIFY noticesChanged)
    // Cubeulator's own row: { name, licence, copyright, text }.
    Q_PROPERTY(QVariantMap own READ own NOTIFY noticesChanged)
    // Every third-party component that ships in this build, by name, as
    // { id, kind, name, version, licence, copyright, text } maps. `licence` is
    // what the app is distributed under (an SPDX identifier, or an expression);
    // `text` is the component's notice and then each licence text it needs,
    // all of it in the words their authors chose.
    Q_PROPERTY(QVariantList components READ components NOTIFY noticesChanged)

public:
    // The embedded resource, or `notices_path` (a test's own file).
    explicit AboutInfo(QString notices_path = defaultNoticesPath(), QObject* parent = nullptr);

    [[nodiscard]] static QString defaultNoticesPath();

    [[nodiscard]] QString name() const;
    [[nodiscard]] QString version() const;
    [[nodiscard]] QString displayVersion() const;
    [[nodiscard]] QString publisher() const;
    [[nodiscard]] QString sourceUrl() const;
    [[nodiscard]] QString licenceId() const;
    [[nodiscard]] bool noticesAvailable() const;
    [[nodiscard]] QVariantMap own() const;
    [[nodiscard]] QVariantList components() const;

    // Reads the notices from `path` instead, on next use: for a test that needs
    // them missing (the screen has a state for it) or its own.
    void setNoticesPath(const QString& path);

signals:
    void noticesChanged();

private:
    void load() const;

    QString notices_path_;
    // Filled by load(), once, from const getters: the screens only read.
    mutable bool loaded_ = false;
    mutable bool available_ = false;
    mutable QVariantMap own_;
    mutable QVariantList components_;
    mutable QString source_url_;
    mutable QString licence_id_;
};

}  // namespace cube::app
