// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "targets.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

using namespace Qt::StringLiterals;

QList<Target> parseTargetsJson(const QByteArray &json)
{
    QList<Target> targets;
    // [{"Node": {"StableID": …, "ComputedName": …, "Addresses": ["100.64.0.1/32", …], "Online": true,
    //            "Hostinfo": {"OS": "linux", …}, …}, "PeerAPIURL": …}, …]
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
        target.online = node["Online"_L1].toBool();
        targets.append(target);
    }
    std::stable_sort(targets.begin(), targets.end(), [](const Target &a, const Target &b) {
        if (a.online != b.online) {
            return a.online;
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return targets;
}
