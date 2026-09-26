// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "targets.h"

#include <QAbstractListModel>
#include <QtQmlIntegration>

class QNetworkAccessManager;
class QNetworkReply;

// Taildrop targets from tailscaled's LocalAPI: the last used one first, then online, then offline devices.
// Also finds out how each device is reached.
class TailscaleTargetsModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    // why there is no list, e.g. tailscaled is not running; empty otherwise
    Q_PROPERTY(QString error READ error NOTIFY loaded)
    // StableID of the last used device when it is online, so the dialog can preselect it; empty otherwise
    Q_PROPERTY(QString preselected READ preselected NOTIFY loaded)

public:
    enum Roles {
        StableIdRole = Qt::UserRole + 1,
        NameRole,
        IpRole,
        OsRole,
        OnlineRole,
        StatusTextRole, // e.g. "offline, last seen yesterday at 17:23"; empty when online
        PathRole, // how an online device is reached, e.g. "direct (192.168.1.20)"; empty otherwise
    };

    explicit TailscaleTargetsModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool loading() const;
    QString error() const;
    QString preselected() const;

    // Whether the device with that StableID is listed and online, i.e. can still be chosen
    Q_INVOKABLE bool isOnline(const QString &stableId) const;
    // Name of the device with that StableID; empty when it is not listed
    Q_INVOKABLE QString nameOf(const QString &stableId) const;

    // Asks tailscaled again; the model keeps its rows until the answer is there
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void loadingChanged();
    void loaded();

private:
    void finishLoading();

    QNetworkAccessManager *m_network;
    QNetworkReply *m_targetsReply = nullptr;
    QNetworkReply *m_statusReply = nullptr;
    QList<Target> m_targets;
    QString m_error;
};
