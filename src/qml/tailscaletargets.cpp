// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tailscaletargets.h"
#include "localapi.h"

#include <KFormat>
#include <KLocalizedString>
#include <QLocale>
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
    for (QNetworkReply *reply : {m_targetsReply, m_statusReply}) {
        if (reply) {
            reply->disconnect(this);
            reply->abort();
            reply->deleteLater();
        }
    }
    const auto watch = [this](QNetworkReply *reply) {
        connect(reply, &QNetworkReply::finished, this, &TailscaleTargetsModel::finishLoading);
        return reply;
    };
    m_targetsReply = watch(m_network->get(request(u"file-targets"_s)));
    m_statusReply = watch(m_network->get(request(u"status"_s)));
    if (!wasLoading) {
        Q_EMIT loadingChanged();
    }
}

void TailscaleTargetsModel::finishLoading()
{
    if (!m_targetsReply->isFinished() || !m_statusReply->isFinished()) {
        return;
    }

    // a 200 cut off in the middle of the list is not an empty list
    const LocalApi::Outcome classified = LocalApi::classify(m_targetsReply);
    const LocalApi::Outcome listed =
        classified == LocalApi::Outcome::Ok && m_targetsReply->error() != QNetworkReply::NoError ? LocalApi::Outcome::Other : classified;
    beginResetModel();
    m_targets = listed == LocalApi::Outcome::Ok ? parseTargetsJson(m_targetsReply->readAll()) : QList<Target>();
    // without it the list only lacks the connection paths
    if (LocalApi::classify(m_statusReply) == LocalApi::Outcome::Ok) {
        readConnectionPaths(m_statusReply->readAll(), m_targets);
    }
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
    m_statusReply->deleteLater();
    m_targetsReply = m_statusReply = nullptr;
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
    case StatusTextRole:
        if (target.online) {
            return QString();
        }
        return target.lastSeen.isValid()
            ? i18nc("@info device status", "offline, last seen %1", KFormat().formatRelativeDateTime(target.lastSeen.toLocalTime(), QLocale::ShortFormat))
            : i18nc("@info device status", "offline");
    case PathRole:
        if (!target.online) {
            return QString();
        }
        if (!target.directAddress.isEmpty()) {
            return i18nc("@info how the device is reached", "direct (%1)", target.directAddress);
        }
        if (!target.peerRelay.isEmpty()) {
            return i18nc("@info how the device is reached, %1 is the IP address of a Tailscale peer relay", "via peer relay (%1)", target.peerRelay);
        }
        if (!target.relay.isEmpty()) {
            return i18nc("@info how the device is reached, %1 is a DERP region such as waw", "via DERP relay (%1)", target.relay);
        }
        return QString();
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
        {StatusTextRole, "statusText"},
        {PathRole, "path"},
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
