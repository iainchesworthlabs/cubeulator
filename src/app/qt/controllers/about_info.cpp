// SPDX-FileCopyrightText: 2026 Iain Chesworth
// SPDX-License-Identifier: GPL-3.0-or-later
#include "controllers/about_info.hpp"

#include <cube/version.hpp>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStringList>

#include <algorithm>

namespace cube::app {

namespace {

// The app's own identity, constants of the build. The publisher is a compile
// definition (CUBEULATOR_PUBLISHER_NAME, from cmake/AppIdentity.cmake, where it
// is one line to change); the source address and the licence are what the
// generated notices say too, and are the answer when they cannot be read.
constexpr auto kSourceUrl = "https://github.com/iainchesworthlabs/cubeulator";
constexpr auto kLicenceId = "GPL-3.0-or-later";

// A blank line, between the parts of a notice.
[[nodiscard]] QString blank_line() {
    return QString(2, QChar(0x0A));
}

// What a component's notice is: its own text (the copyright line and the licence
// its authors shipped), then each licence text it is distributed under that the
// generator keeps once for all, under a line that says which.
[[nodiscard]] QString notice_of(const QJsonObject& component, const QJsonObject& licence_texts) {
    QString text = component.value(QStringLiteral("text")).toString().trimmed();
    const QString copyright = component.value(QStringLiteral("copyright")).toString().trimmed();
    if (text.isEmpty() && !copyright.isEmpty()) {
        text = copyright;
    }
    for (const QJsonValue& ref : component.value(QStringLiteral("licenseRefs")).toArray()) {
        const QString id = ref.toString();
        const QString body = licence_texts.value(id).toString().trimmed();
        if (body.isEmpty()) {
            continue;
        }
        if (!text.isEmpty()) {
            text += blank_line();
        }
        text += QStringLiteral("-- ") + id + QStringLiteral(" --") + blank_line() + body;
    }
    return text;
}

}  // namespace

AboutInfo::AboutInfo(QString notices_path, QObject* parent)
    : QObject(parent), notices_path_(std::move(notices_path)) {}

QString AboutInfo::defaultNoticesPath() {
    return QStringLiteral(":/licences/third-party-notices.json");
}

QString AboutInfo::name() const {
    return QStringLiteral("Cubeulator");
}

QString AboutInfo::version() const {
    return QString::fromUtf8(cube::version_string.data(),
                             static_cast<qsizetype>(cube::version_string.size()));
}

QString AboutInfo::displayVersion() const {
    // Isolated left to right (U+2066 ... U+2069): in a right-to-left line the
    // parts of "0.1.0-beta.1" would otherwise change places round the hyphen.
    return QString(QChar(0x2066)) + version() + QString(QChar(0x2069));
}

QString AboutInfo::publisher() const {
    return QString::fromUtf8(CUBEULATOR_PUBLISHER_NAME);
}

QString AboutInfo::sourceUrl() const {
    load();
    return source_url_;
}

QString AboutInfo::licenceId() const {
    load();
    return licence_id_;
}

bool AboutInfo::noticesAvailable() const {
    load();
    return available_;
}

QVariantMap AboutInfo::own() const {
    load();
    return own_;
}

QVariantList AboutInfo::components() const {
    load();
    return components_;
}

void AboutInfo::setNoticesPath(const QString& path) {
    notices_path_ = path;
    loaded_ = false;
    available_ = false;
    own_.clear();
    components_.clear();
    emit noticesChanged();
}

void AboutInfo::load() const {
    if (loaded_) {
        return;
    }
    loaded_ = true;
    source_url_ = QString::fromLatin1(kSourceUrl);
    licence_id_ = QString::fromLatin1(kLicenceId);

    QFile file(notices_path_);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    const QJsonObject root = document.object();
    const QJsonArray components = root.value(QStringLiteral("components")).toArray();
    if (components.isEmpty()) {
        // The placeholder the build writes before the real notices exist.
        return;
    }
    const QJsonObject licence_texts = root.value(QStringLiteral("licenseTexts")).toObject();

    const QJsonObject project = root.value(QStringLiteral("project")).toObject();
    if (const QString source = project.value(QStringLiteral("source")).toString();
        !source.isEmpty()) {
        source_url_ = source;
    }
    if (const QString licence = project.value(QStringLiteral("license")).toString();
        !licence.isEmpty()) {
        licence_id_ = licence;
    }
    {
        const QString copyright = project.value(QStringLiteral("copyright")).toString();
        QString text = copyright;
        if (const QString body = licence_texts.value(licence_id_).toString().trimmed();
            !body.isEmpty()) {
            text += blank_line() + QStringLiteral("-- ") + licence_id_ + QStringLiteral(" --") +
                    blank_line() + body;
        }
        own_ = {{QStringLiteral("name"), project.value(QStringLiteral("name")).toString()},
                {QStringLiteral("licence"), licence_id_},
                {QStringLiteral("copyright"), copyright},
                {QStringLiteral("text"), text}};
    }

    for (const QJsonValue& value : components) {
        const QJsonObject component = value.toObject();
        const QString name = component.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) {
            continue;
        }
        // What the app is distributed under is what a reader wants; the
        // expression the component offers (Qt's "A OR B OR C") is in its text.
        QString licence = component.value(QStringLiteral("distributedUnder")).toString();
        if (licence.isEmpty()) {
            licence = component.value(QStringLiteral("license")).toString();
        }
        components_.append(QVariantMap{
            {QStringLiteral("id"), component.value(QStringLiteral("id")).toString()},
            {QStringLiteral("kind"), component.value(QStringLiteral("kind")).toString()},
            {QStringLiteral("name"), name},
            {QStringLiteral("version"), component.value(QStringLiteral("version")).toString()},
            {QStringLiteral("licence"), licence},
            {QStringLiteral("copyright"), component.value(QStringLiteral("copyright")).toString()},
            {QStringLiteral("text"), notice_of(component, licence_texts)}});
    }
    std::stable_sort(components_.begin(), components_.end(),
                     [](const QVariant& a, const QVariant& b) {
                         return QString::compare(a.toMap().value(QStringLiteral("name")).toString(),
                                                 b.toMap().value(QStringLiteral("name")).toString(),
                                                 Qt::CaseInsensitive) < 0;
                     });
    available_ = !components_.isEmpty();
}

}  // namespace cube::app
