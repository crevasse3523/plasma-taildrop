// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

// The HTTP API tailscaled serves on its unix socket. It works without root for the user set with
// `tailscale set --operator=$USER`; the other users can only read, e.g. list the file targets.
namespace LocalApi
{
// What the user runs to become the operator; not translated, the shell needs it as it is
inline const QString OperatorCommand = QStringLiteral("sudo tailscale set --operator=$USER");

// PLASMA_TAILDROP_SOCKET when set, otherwise the first socket that exists, otherwise /run/tailscale/tailscaled.sock
QString socketPath();

// A request for /localapi/v0/<path> over the socket; path is already percent-encoded, e.g. "file-targets"
QNetworkRequest request(const QString &path);

// The path for PUT file-put: sends fileName (without folders) to the device with that StableID
QString filePutPath(const QString &stableId, const QString &fileName);

enum class Outcome {
    Ok,
    NotOperator, // 403 from tailscaled itself: this user may not send files, see `tailscale set --operator`
    DeviceRefused, // 403 from the device, passed on by tailscaled: it takes no files, e.g. Taildrop is off there
    NodeNotFound, // 404: no such device, or it does not accept files
    PeerUnreachable, // 502, 503, 504 or a timeout: tailscaled could not reach the device
    DaemonDown, // no socket, or nobody listening on it
    Cancelled, // aborted by us, or QNetworkRequest::setTransferTimeout() ran out: Qt reports both the same way
    Other,
};

// Sorts the result of a finished reply; httpStatus is 0 when there was no HTTP response, body is what tailscaled
// replied, which tells its own refusal from the device's
Outcome classify(int httpStatus, QNetworkReply::NetworkError error, QByteArrayView body);

// The same for a finished reply; peeks at the body, so it can still be read
Outcome classify(QNetworkReply *reply);

// Why a finished reply failed: what tailscaled replied, or Qt's error when it replied nothing
QString message(QNetworkReply *reply);
}
