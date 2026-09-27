// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tailscaletargets.h"
#include "localapi.h"
#include "settings.h"

#include <KFormat>
#include <KLocalizedString>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QSettings>
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
    for (QNetworkReply *reply : {m_targetsReply, m_statusReply, m_probeReply}) {
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
    // An empty upload to a device that cannot exist: tailscaled checks the permission to send (403) before it
    // looks for the device (404), so this tells whether sending would be allowed without sending anything
    m_probeReply = watch(m_network->put(request(LocalApi::filePutPath(u"plasma-taildrop-probe"_s, u"probe"_s)), QByteArray()));
    if (!wasLoading) {
        Q_EMIT loadingChanged();
    }
}

void TailscaleTargetsModel::finishLoading()
{
    if (!m_targetsReply->isFinished() || !m_statusReply->isFinished() || !m_probeReply->isFinished()) {
        return;
    }

    const QString lastUsedId = QSettings(Settings::Name).value(Settings::LastTargetIdKey).toString();

    // a 200 cut off in the middle of the list is not an empty list
    const LocalApi::Outcome classified = LocalApi::classify(m_targetsReply);
    const LocalApi::Outcome listed =
        classified == LocalApi::Outcome::Ok && m_targetsReply->error() != QNetworkReply::NoError ? LocalApi::Outcome::Other : classified;
    beginResetModel();
    m_targets = listed == LocalApi::Outcome::Ok ? parseTargetsJson(m_targetsReply->readAll(), lastUsedId) : QList<Target>();
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

    const LocalApi::Outcome probed = LocalApi::classify(m_probeReply);
    m_canSend = probed == LocalApi::Outcome::NodeNotFound;
    if (m_canSend || !m_error.isEmpty()) {
        m_permissionHint.clear();
    } else if (probed == LocalApi::Outcome::NotOperator) {
        m_permissionHint = i18n("Tailscale only lets its operator send files. To make yourself the operator, run: %1", LocalApi::OperatorCommand);
    } else {
        m_permissionHint = i18n("Tailscale does not accept files to send: %1", LocalApi::message(m_probeReply));
    }

    m_targetsReply->deleteLater();
    m_statusReply->deleteLater();
    m_probeReply->deleteLater();
    m_targetsReply = m_statusReply = m_probeReply = nullptr;
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

bool TailscaleTargetsModel::canSend() const
{
    return m_canSend;
}

QString TailscaleTargetsModel::permissionHint() const
{
    return m_permissionHint;
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

QString TailscaleTargetsModel::preselected() const
{
    if (!m_targets.isEmpty() && m_targets.first().lastUsed && m_targets.first().online) {
        return m_targets.first().stableId;
    }
    return {};
}
