// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QHash>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTimer>

// Stands in for tailscaled: a unix socket speaking just enough HTTP/1.1 (Content-Length bodies, keep-alive)
// for QNetworkAccessManager. Records every request and answers with a scripted reply per path.
// Points PLASMA_TAILDROP_SOCKET at itself while it exists.
class FakeLocalApi
{
public:
    struct Request {
        QByteArray method;
        QByteArray path; // as sent, e.g. "/localapi/v0/file-put/nA/a%20b.txt"
        QHash<QByteArray, QByteArray> headers; // names in lower case
        QByteArray body;
    };

    FakeLocalApi()
    {
        m_server.listen(m_dir.filePath(QStringLiteral("tailscaled.sock")));
        qputenv("PLASMA_TAILDROP_SOCKET", m_server.fullServerName().toLocal8Bit());
        QObject::connect(&m_server, &QLocalServer::newConnection, &m_server, [this] {
            while (QLocalSocket *socket = m_server.nextPendingConnection()) {
                m_buffers.insert(socket, {});
                socket->setReadBufferSize(m_throttle);
                QObject::connect(socket, &QLocalSocket::disconnected, socket, [this, socket] {
                    m_buffers.remove(socket);
                    socket->deleteLater();
                });
                QObject::connect(socket, &QLocalSocket::readyRead, socket, [this, socket] {
                    if (m_throttle == 0) {
                        serve(socket);
                    }
                });
            }
        });
        QObject::connect(&m_pump, &QTimer::timeout, &m_server, [this] {
            const QList<QLocalSocket *> sockets = m_buffers.keys();
            for (QLocalSocket *socket : sockets) {
                serve(socket);
            }
        });
    }

    ~FakeLocalApi()
    {
        qunsetenv("PLASMA_TAILDROP_SOCKET");
    }

    bool isListening() const
    {
        return m_server.isListening();
    }

    // Answers requests for path (under /localapi/v0/, without a query) with status, body and extra header lines
    // such as "Location: /x\r\n". Status 0 never answers, like a peer that stalls. Unscripted paths get 404.
    void respond(const QByteArray &path, int status, const QByteArray &body = {}, const QByteArray &headers = {})
    {
        m_replies.insert(path, {status, body, headers});
    }

    // Writes raw as soon as the request headers are in, without reading the body, then closes the connection,
    // like tailscaled refusing early or dying mid-reply
    void respondRaw(const QByteArray &path, const QByteArray &raw)
    {
        m_replies.insert(path, {-1, raw, {}});
    }

    // Reads at most bytesPerTick every 10 ms, so that an upload lasts long enough to report progress several times
    void throttle(qint64 bytesPerTick)
    {
        m_throttle = bytesPerTick;
        m_pump.start(10);
    }

    QList<Request> requests;

private:
    struct Reply {
        int status = 404; // -1: body is the raw reply
        QByteArray body;
        QByteArray headers;
    };

    void serve(QLocalSocket *socket)
    {
        QByteArray &buffer = m_buffers[socket];
        buffer += m_throttle == 0 ? socket->readAll() : socket->read(m_throttle);
        for (;;) {
            const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
            if (headerEnd < 0) {
                return;
            }
            const QList<QByteArray> lines = buffer.left(headerEnd).split('\n');
            Request request;
            const QList<QByteArray> requestLine = lines.first().trimmed().split(' ');
            request.method = requestLine.value(0);
            request.path = requestLine.value(1);
            for (const QByteArray &line : lines.mid(1)) {
                const qsizetype colon = line.indexOf(':');
                request.headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
            }
            const Reply reply = m_replies.value(request.path.mid(qsizetype(sizeof("/localapi/v0/")) - 1).split('?').first());
            if (reply.status == -1) {
                requests.append(request);
                m_buffers.remove(socket);
                socket->write(reply.body);
                socket->disconnectFromServer();
                return;
            }
            const qsizetype length = request.headers.value("content-length").toLongLong();
            if (buffer.size() < headerEnd + 4 + length) {
                return;
            }
            request.body = buffer.mid(headerEnd + 4, length);
            buffer.remove(0, headerEnd + 4 + length);
            requests.append(request);

            if (reply.status != 0) {
                socket->write("HTTP/1.1 " + QByteArray::number(reply.status) + " Scripted\r\nContent-Type: text/plain\r\n" + reply.headers
                              + "Content-Length: " + QByteArray::number(reply.body.size()) + "\r\n\r\n" + reply.body);
            }
        }
    }

    QTemporaryDir m_dir;
    QHash<QByteArray, Reply> m_replies;
    QHash<QLocalSocket *, QByteArray> m_buffers;
    qint64 m_throttle = 0;
    QTimer m_pump;
    // last, so that its sockets disconnect while the rest still exists
    QLocalServer m_server;
};
