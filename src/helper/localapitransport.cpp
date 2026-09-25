// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "localapitransport.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>

LocalApiTransport::LocalApiTransport(QObject *parent)
    : Transport(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

bool LocalApiTransport::send(const QString &stableId, const QString &filePath, const QString &fileName, QString *errorString)
{
    auto file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        *errorString = file->errorString();
        delete file;
        return false;
    }
    QNetworkRequest request = LocalApi::request(LocalApi::filePutPath(stableId, fileName));
    request.setHeader(QNetworkRequest::ContentLengthHeader, file->size());
    // read the file as it goes out instead of all of it into memory first
    request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
    m_reply = m_network->put(request, file);
    // closed and deleted with the reply
    file->setParent(m_reply);
    connect(m_reply, &QNetworkReply::uploadProgress, this, [this](qint64 bytesSent, qint64 bytesTotal) {
        // Qt also reports 0 of 0 before it knows the size
        if (bytesTotal > 0) {
            Q_EMIT progress(bytesSent, bytesTotal);
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        const LocalApi::Outcome outcome = LocalApi::classify(m_reply);
        const QString message = outcome == LocalApi::Outcome::Ok ? QString() : LocalApi::message(m_reply);
        release();
        Q_EMIT finished(outcome, message);
    });
    return true;
}

void LocalApiTransport::abort()
{
    if (!m_reply) {
        return;
    }
    // abort() emits finished() right away, which must not reach the queue any more
    disconnect(m_reply, nullptr, this, nullptr);
    m_reply->abort();
    release();
}

void LocalApiTransport::release()
{
    m_reply->deleteLater();
    m_reply = nullptr;
}
