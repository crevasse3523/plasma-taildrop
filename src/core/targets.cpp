// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "targets.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <algorithm>

using namespace Qt::StringLiterals;

namespace
{
// "ip:port", "[ipv6]:port"
QString host(const QString &address)
{
    return QUrl(u"//"_s + address).host();
}
}

QList<Target> parseTargetsJson(const QByteArray &json, const QString &lastUsedId)
{
    QList<Target> targets;
    // [{"Node": {"StableID": …, "ComputedName": …, "Addresses": ["100.64.0.1/32", …], "Online": true,
    //            "LastSeen": "2026-09-12T17:23:59.1Z", "Hostinfo": {"OS": "linux", …}, …}, "PeerAPIURL": …}, …]
    for (const QJsonValue &entry : QJsonDocument::fromJson(json).array()) {
        const QJsonObject node = entry["Node"_L1].toObject();
        Target target;
        target.stableId = node["StableID"_L1].toString();
        target.name = node["ComputedName"_L1].toString();
        if (target.stableId.isEmpty() || target.name.isEmpty()) {
            continue;
        }
        target.ip = node["Addresses"_L1][0].toString().section(u'/', 0, 0);
        target.os = node["Hostinfo"_L1]["OS"_L1].toString();
        target.lastSeen = QDateTime::fromString(node["LastSeen"_L1].toString(), Qt::ISODateWithMs);
        target.online = node["Online"_L1].toBool();
        target.lastUsed = target.stableId == lastUsedId;
        targets.append(target);
    }
    std::stable_sort(targets.begin(), targets.end(), [](const Target &a, const Target &b) {
        if (a.lastUsed != b.lastUsed) {
            return a.lastUsed;
        }
        if (a.online != b.online) {
            return a.online;
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return targets;
}

void readConnectionPaths(const QByteArray &statusJson, QList<Target> &targets)
{
    // {"Peer": {"nodekey:…": {"ID": "<StableID>", "CurAddr": "192.168.1.20:41641", "PeerRelay": "198.51.100.7:7777:vni:3",
    //                        "Relay": "waw", …}, …}, "Self": …, …}
    const QJsonObject peers = QJsonDocument::fromJson(statusJson)["Peer"_L1].toObject();
    for (const QJsonValue &peer : peers) {
        const QString stableId = peer["ID"_L1].toString();
        for (Target &target : targets) {
            if (!stableId.isEmpty() && target.stableId == stableId) {
                target.directAddress = host(peer["CurAddr"_L1].toString());
                target.peerRelay = host(peer["PeerRelay"_L1].toString().section(u":vni:"_s, 0, 0));
                target.relay = peer["Relay"_L1].toString();
            }
        }
    }
}
