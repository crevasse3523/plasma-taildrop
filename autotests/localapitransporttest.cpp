// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// The uploads of plasma-taildrop-send against FakeLocalApi, held to the contract the send queue relies on

#include "localapitransport.h"
#include "fakelocalapi.h"

#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using LocalApi::Outcome;

class LocalApiTransportTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString writeFile(const QString &name, const QByteArray &contents)
    {
        QFile file(m_dir.filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()) {
            return {};
        }
        return file.fileName();
    }

private Q_SLOTS:
    void sendsFile()
    {
        FakeLocalApi fake;
        fake.respond("file-put/nA/za%C5%BC%C3%B3%C5%82%C4%87%20g%C4%99%C5%9Bl%C4%85.bin", 200);
        fake.throttle(128 * 1024);
        QByteArray contents(3 * 1024 * 1024, Qt::Uninitialized);
        QRandomGenerator(7).fillRange(reinterpret_cast<quint32 *>(contents.data()), contents.size() / sizeof(quint32));
        const QString path = writeFile(u"local.bin"_s, contents);
        QVERIFY(!path.isEmpty());

        LocalApiTransport transport;
        QSignalSpy finished(&transport, &Transport::finished);
        QList<qint64> progress;
        connect(&transport, &Transport::progress, this, [&progress](qint64 bytesSent, qint64 bytesTotal) {
            QCOMPARE(bytesTotal, 3 * 1024 * 1024);
            progress << bytesSent;
        });
        QString errorString;
        QVERIFY(transport.send(u"nA"_s, path, u"zażółć gęślą.bin"_s, &errorString));
        QVERIFY(finished.wait(20000));
        QCOMPARE(finished.first().at(0).value<Outcome>(), Outcome::Ok);
        QCOMPARE(finished.first().at(1).toString(), QString());

        QCOMPARE(fake.requests.size(), 1);
        QCOMPARE(fake.requests.first().method, "PUT");
        QCOMPARE(fake.requests.first().headers.value("content-length"), QByteArray::number(contents.size()));
        QVERIFY(fake.requests.first().body == contents);
        QVERIFY2(progress.size() > 2, qPrintable(QString::number(progress.size())));
        QVERIFY(std::is_sorted(progress.cbegin(), progress.cend()));
        QCOMPARE(progress.last(), contents.size());
    }

    void refusal()
    {
        FakeLocalApi fake;
        fake.respond("file-put/nA/a.txt", 403, "file access denied\n");
        const QString path = writeFile(u"a.txt"_s, "a");
        LocalApiTransport transport;
        QSignalSpy finished(&transport, &Transport::finished);
        QString errorString;
        QVERIFY(transport.send(u"nA"_s, path, u"a.txt"_s, &errorString));
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).value<Outcome>(), Outcome::NotOperator);
        // what tailscaled said, for the notification
        QCOMPARE(finished.first().at(1).toString(), u"file access denied"_s);
    }

    // tailscaled passes on the 403 of a device that takes no files, which is not about the operator
    void deviceRefusal()
    {
        FakeLocalApi fake;
        fake.respond("file-put/nA/a.txt", 403, "Taildrop disabled; no storage directory\n");
        const QString path = writeFile(u"a.txt"_s, "a");
        LocalApiTransport transport;
        QSignalSpy finished(&transport, &Transport::finished);
        QString errorString;
        QVERIFY(transport.send(u"nA"_s, path, u"a.txt"_s, &errorString));
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).value<Outcome>(), Outcome::DeviceRefused);
        QCOMPARE(finished.first().at(1).toString(), u"Taildrop disabled; no storage directory"_s);
    }

    // The device's reply comes back through tailscaled, so it must not be able to send the file elsewhere
    void redirect()
    {
        FakeLocalApi fake;
        fake.respond("file-put/nA/a.txt", 307, {}, "Location: /localapi/v0/file-put/nB/a.txt\r\n");
        fake.respond("file-put/nB/a.txt", 200);
        const QString path = writeFile(u"a.txt"_s, "a");
        LocalApiTransport transport;
        QSignalSpy finished(&transport, &Transport::finished);
        QString errorString;
        QVERIFY(transport.send(u"nA"_s, path, u"a.txt"_s, &errorString));
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).value<Outcome>(), Outcome::Other);
        QCOMPARE(fake.requests.size(), 1);
    }

    void noDaemon()
    {
        qputenv("PLASMA_TAILDROP_SOCKET", m_dir.filePath(u"missing.sock"_s).toLocal8Bit());
        const QString path = writeFile(u"a.txt"_s, "a");
        LocalApiTransport transport;
        QSignalSpy finished(&transport, &Transport::finished);
        QString errorString;
        QVERIFY(transport.send(u"nA"_s, path, u"a.txt"_s, &errorString));
        // never from within send()
        QCOMPARE(finished.count(), 0);
        QVERIFY(finished.wait());
        qunsetenv("PLASMA_TAILDROP_SOCKET");
        QCOMPARE(finished.first().at(0).value<Outcome>(), Outcome::DaemonDown);
        QVERIFY(!finished.first().at(1).toString().isEmpty());
    }

    void unreadable()
    {
        FakeLocalApi fake;
        LocalApiTransport transport;
        QSignalSpy finished(&transport, &Transport::finished);
        QString errorString;
        QVERIFY(!transport.send(u"nA"_s, m_dir.filePath(u"missing.txt"_s), u"missing.txt"_s, &errorString));
        QVERIFY(!errorString.isEmpty());
        QTest::qWait(50);
        QCOMPARE(finished.count(), 0);
        QVERIFY(fake.requests.isEmpty());
    }

    void abort()
    {
        FakeLocalApi fake;
        fake.respond("file-put/nA/stall.txt", 0);
        fake.respond("file-put/nA/next.txt", 200);
        LocalApiTransport transport;
        QSignalSpy finished(&transport, &Transport::finished);
        QString errorString;
        QVERIFY(transport.send(u"nA"_s, writeFile(u"stall.txt"_s, "stall"), u"stall.txt"_s, &errorString));
        QTRY_COMPARE(fake.requests.size(), 1);
        transport.abort();
        QTest::qWait(50);
        QCOMPARE(finished.count(), 0);

        // and the next upload goes on as usual
        QVERIFY(transport.send(u"nA"_s, writeFile(u"next.txt"_s, "next"), u"next.txt"_s, &errorString));
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).value<Outcome>(), Outcome::Ok);
        QCOMPARE(fake.requests.last().body, "next");
    }
};

QTEST_GUILESS_MAIN(LocalApiTransportTest)

#include "localapitransporttest.moc"
