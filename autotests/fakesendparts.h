// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "sendqueue.h"

#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

// The collaborators of SendQueue, faked: they only record the calls, and the tests emit their signals

class FakeTransport : public Transport
{
public:
    struct Send {
        QString stableId;
        QString filePath;
        QString fileName;
    };
    QList<Send> sends;
    QStringList unreadable;
    int aborts = 0;

    bool send(const QString &stableId, const QString &filePath, const QString &fileName, QString *errorString) override
    {
        if (unreadable.contains(filePath)) {
            *errorString = QStringLiteral("Permission denied");
            return false;
        }
        sends.append({stableId, filePath, fileName});
        return true;
    }

    void abort() override
    {
        ++aborts;
    }
};

// A queue over the fakes, and files for it to send; the test's init() and cleanup() call initQueue() and cleanupQueue()
class SendQueueFixture
{
protected:
    QTemporaryDir m_dir;
    FakeTransport *m_transport = nullptr;
    SendQueue *m_queue = nullptr;

    void initQueue()
    {
        if (!m_dir.isValid()) {
            qFatal("no temporary folder");
        }
        m_transport = new FakeTransport;
        m_queue = new SendQueue(m_transport);
    }

    void cleanupQueue()
    {
        delete m_queue;
        delete m_transport;
    }

    // A file with size bytes in the test folder
    QString file(const QString &name, qint64 size = 10)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(QByteArray(size, 'x')) != size) {
            qFatal("cannot write %s", qPrintable(path));
        }
        return path;
    }

    void finish(LocalApi::Outcome outcome = LocalApi::Outcome::Ok, const QString &message = {})
    {
        Q_EMIT m_transport->finished(outcome, message);
    }
};
