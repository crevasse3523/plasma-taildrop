// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "localapi.h"

#include <QFileInfo>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace LocalApi
{
QString socketPath()
{
    const QString overridden = qEnvironmentVariable("PLASMA_TAILDROP_SOCKET");
    if (!overridden.isEmpty()) {
        return overridden;
    }
    const QString path = u"/run/tailscale/tailscaled.sock"_s;
    const QString legacyPath = u"/var/run/tailscale/tailscaled.sock"_s;
    return !QFileInfo::exists(path) && QFileInfo::exists(legacyPath) ? legacyPath : path;
}

QNetworkRequest request(const QString &path)
{
    // tailscaled checks the Host header; the URL host is what Qt sends in it
    QNetworkRequest request(QUrl(u"unix+http://local-tailscaled.sock/localapi/v0/"_s + path));
    request.setAttribute(QNetworkRequest::FullLocalServerNameAttribute, socketPath());
    // tailscaled passes on the device's replies, and a device must not move an upload elsewhere
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    return request;
}

QString filePutPath(const QString &stableId, const QString &fileName)
{
    return u"file-put/"_s + QString::fromLatin1(QUrl::toPercentEncoding(stableId)) + u'/' + QString::fromLatin1(QUrl::toPercentEncoding(fileName));
}

Outcome classify(int httpStatus, QNetworkReply::NetworkError error, QByteArrayView body)
{
    // Qt 6.11 reports a transfer timeout as TimeoutError, older versions like abort()
    if (error == QNetworkReply::OperationCanceledError || error == QNetworkReply::TimeoutError) {
        return Outcome::Cancelled;
    }
    switch (httpStatus) {
    case 0:
        break;
    case 200:
        return Outcome::Ok;
    case 403:
        // tailscaled passes on the device's own replies, and the device says 403 when it takes no files
        return body.trimmed() == "file access denied" ? Outcome::NotOperator : Outcome::DeviceRefused;
    case 404:
        return Outcome::NodeNotFound;
    case 502:
    case 503:
    case 504:
        return Outcome::PeerUnreachable;
    default:
        // e.g. a 500 from the device that could not store the file
        return Outcome::Other;
    }
    switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::HostNotFoundError:
        return Outcome::DaemonDown;
    default:
        return Outcome::Other;
    }
}

Outcome classify(QNetworkReply *reply)
{
    return classify(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), reply->error(), reply->peek(reply->bytesAvailable()));
}

QString message(QNetworkReply *reply)
{
    // a 200 body is data, not an explanation, even when the reply broke off
    if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
        return reply->errorString();
    }
    // tailscaled explains its errors in the body; the device's can be of any length and ends up in notifications
    constexpr qint64 MaxBytes = 1024;
    QString body = QString::fromUtf8(reply->peek(MaxBytes)).trimmed();
    if (reply->bytesAvailable() > MaxBytes) {
        body += u'…';
    }
    return body.isEmpty() ? reply->errorString() : body;
}
}
