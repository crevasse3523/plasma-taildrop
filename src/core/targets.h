// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

// A device that accepts Taildrop files
struct Target {
    QString stableId; // e.g. "nAeGTLLAjt11CNTRL", what file-put addresses
    QString name; // ComputedName, as shown by `tailscale status`
    QString ip; // first Tailscale address, without the prefix length
    QString os; // Hostinfo.OS, e.g. "linux", "windows", "android"
    QDateTime lastSeen; // only known for offline devices
    QString directAddress; // IP address of a direct connection (LAN or internet), empty when there is none
    QString peerRelay; // IP address of the Tailscale peer relay the device is reached through, if any
    QString relay; // DERP region the device is reached through without a direct connection, e.g. "waw"
    bool online = false;
};

// Parses the reply of GET /localapi/v0/file-targets and orders it for display:
// online, then offline devices, alphabetically within each group.
// Entries without a StableID or a name are skipped; malformed JSON gives an empty list.
QList<Target> parseTargetsJson(const QByteArray &json);

// Fills directAddress, peerRelay and relay of the targets from the reply of GET /localapi/v0/status; malformed JSON changes nothing
void readConnectionPaths(const QByteArray &statusJson, QList<Target> &targets);
