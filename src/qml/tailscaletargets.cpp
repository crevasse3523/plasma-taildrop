// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tailscaletargets.h"
#include "localapi.h"

#include <KLocalizedString>
#include <QNetworkAccessManager>
#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
// tailscaled answers from memory; anything slower means it is stuck
constexpr int TimeoutMs = 10000;

QNetworkRequest request(const QString &path)
{
    QNetworkRequest request = LocalApi::request(path);
    request.setTransferTimeout(TimeoutMs);
    return request;
}
}

TailscaleTargetsModel::TailscaleTargetsModel(QObject *parent)
    : QAbstractListModel(parent)
    // a child, so that its replies are gone before they could report to a half destroyed model
    , m_network(new QNetworkAccessManager(this))
{
    reload();
}

void TailscaleTargetsModel::reload()
{
    const bool wasLoading = loading();
    if (m_targetsReply) {
        m_targetsReply->disconnect(this);
        m_targetsReply->abort();
        m_targetsReply->deleteLater();
    }
    m_targetsReply = m_network->get(request(u"file-targets"_s));
    connect(m_targetsReply, &QNetworkReply::finished, this, &TailscaleTargetsModel::finishLoading);
    if (!wasLoading) {
        Q_EMIT loadingChanged();
    }
}

void TailscaleTargetsModel::finishLoading()
{
    // a 200 cut off in the middle of the list is not an empty list
    const LocalApi::Outcome classified = LocalApi::classify(m_targetsReply);
    const LocalApi::Outcome listed =
        classified == LocalApi::Outcome::Ok && m_targetsReply->error() != QNetworkReply::NoError ? LocalApi::Outcome::Other : classified;
    beginResetModel();
    m_targets = listed == LocalApi::Outcome::Ok ? parseTargetsJson(m_targetsReply->readAll()) : QList<Target>();
    endResetModel();

    switch (listed) {
    case LocalApi::Outcome::Ok:
        m_error.clear();
        break;
    case LocalApi::Outcome::DaemonDown:
        m_error = i18n("Tailscale is not running.");
        break;
    case LocalApi::Outcome::Cancelled: // only the transfer timeout aborts a reply that is still watched
        m_error = i18n("Tailscale did not answer in time.");
        break;
    default:
        m_error = i18n("Could not list the devices: %1", LocalApi::message(m_targetsReply));
    }

    m_targetsReply->deleteLater();
    m_targetsReply = nullptr;
    Q_EMIT loadingChanged();
    Q_EMIT loaded();
}

int TailscaleTargetsModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_targets.size();
}

QVariant TailscaleTargetsModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Target &target = m_targets[index.row()];
    switch (role) {
    case StableIdRole:
        return target.stableId;
    case NameRole:
        return target.name;
    case IpRole:
        return target.ip;
    case OsRole:
        return target.os;
    case OnlineRole:
        return target.online;
    }
    return {};
}

QHash<int, QByteArray> TailscaleTargetsModel::roleNames() const
{
    return {
        {StableIdRole, "stableId"},
        {NameRole, "name"},
        {IpRole, "ip"},
        {OsRole, "os"},
        {OnlineRole, "online"},
    };
}

bool TailscaleTargetsModel::loading() const
{
    return m_targetsReply != nullptr;
}

QString TailscaleTargetsModel::error() const
{
    return m_error;
}

bool TailscaleTargetsModel::isOnline(const QString &stableId) const
{
    return std::any_of(m_targets.cbegin(), m_targets.cend(), [&stableId](const Target &target) {
        return target.online && target.stableId == stableId;
    });
}

QString TailscaleTargetsModel::nameOf(const QString &stableId) const
{
    const auto target = std::find_if(m_targets.cbegin(), m_targets.cend(), [&stableId](const Target &target) {
        return target.stableId == stableId;
    });
    return target != m_targets.cend() ? target->name : QString();
}
