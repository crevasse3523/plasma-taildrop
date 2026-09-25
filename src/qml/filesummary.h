// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQmlIntegration>

// What the Share dialog is about to send
class FileSummary : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    // Returns {text, hasFolders, problem} for the shared URLs: text is the dialog heading, such as
    // "Send 3 files (1.2 MiB) to:", and problem says why they cannot be sent, or is empty
    Q_INVOKABLE QVariantMap summarize(const QStringList &urls) const;
};
