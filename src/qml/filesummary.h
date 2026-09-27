// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQmlIntegration>

// What the Share dialog is about to send, and how shared folders get packed
class FileSummary : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // the formats folders can be packed into, e.g. "zip" and "tar.zst"
    Q_PROPERTY(QStringList archiveFormats READ archiveFormats CONSTANT)
    // the format picked last time, remembered in the settings
    Q_PROPERTY(QString archiveFormat READ archiveFormat WRITE setArchiveFormat NOTIFY archiveFormatChanged)

public:
    using QObject::QObject;

    // Returns {text, hasFolders, problem} for the shared URLs: text is the dialog heading, such as
    // "Send 3 files (1.2 MiB) to:", and problem says why they cannot be sent, or is empty
    Q_INVOKABLE QVariantMap summarize(const QStringList &urls) const;

    QStringList archiveFormats() const;
    QString archiveFormat() const;
    void setArchiveFormat(const QString &format);

Q_SIGNALS:
    void archiveFormatChanged();
};
