// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Talks HTTP over the unix socket to FakeLocalApi the way the plugin talks to tailscaled

#include "localapi.h"
#include "fakelocalapi.h"

#include <QNetworkAccessManager>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>
#include <memory>

using namespace Qt::StringLiterals;
using LocalApi::Outcome;

class LocalApiTest : public QObject
{
    Q_OBJECT

    QNetworkAccessManager m_nam;

    // Waits for the reply and sorts it the way the callers do
    static Outcome finish(QNetworkReply *reply)
    {
        if (!reply->isFinished()) {
            QSignalSpy finished(reply, &QNetworkReply::finished);
            if (!finished.wait(20000)) {
                return Outcome::Other;
            }
        }
        return LocalApi::classify(reply);
    }

private Q_SLOTS:
    void init()
    {
        // every test has its own socket, but Qt keeps connections by host name only
        m_nam.clearConnectionCache();
    }

    void getHeaders()
    {
        FakeLocalApi fake;
        QVERIFY(fake.isListening());
        fake.respond("file-targets", 200, "[]");
        std::unique_ptr<QNetworkReply> reply(m_nam.get(LocalApi::request(u"file-targets"_s)));
        QCOMPARE(finish(reply.get()), Outcome::Ok);
        QCOMPARE(reply->readAll(), "[]");

        QCOMPARE(fake.requests.size(), 1);
        const FakeLocalApi::Request &request = fake.requests.first();
        QCOMPARE(request.method, "GET");
        QCOMPARE(request.path, "/localapi/v0/file-targets");
        // tailscaled refuses requests with another Host, and any with Origin or Referer (browsers)
        QCOMPARE(request.headers.value("host"), "local-tailscaled.sock");
        QVERIFY(!request.headers.contains("origin"));
        QVERIFY(!request.headers.contains("referer"));
    }

    void streamedPut()
    {
        FakeLocalApi fake;
        fake.respond("file-put/nA/big.bin", 200);
        fake.throttle(256 * 1024);

        QTemporaryFile file;
        QVERIFY(file.open());
        QByteArray contents(5 * 1024 * 1024, Qt::Uninitialized);
        QRandomGenerator(42).fillRange(reinterpret_cast<quint32 *>(contents.data()), contents.size() / sizeof(quint32));
        QCOMPARE(file.write(contents), contents.size());
        QVERIFY(file.seek(0));

        QNetworkRequest request = LocalApi::request(LocalApi::filePutPath(u"nA"_s, u"big.bin"_s));
        request.setHeader(QNetworkRequest::ContentLengthHeader, file.size());
        request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
        std::unique_ptr<QNetworkReply> reply(m_nam.put(request, &file));
        QList<qint64> progress;
        connect(reply.get(), &QNetworkReply::uploadProgress, this, [&progress](qint64 sent, qint64 total) {
            if (total > 0) {
                progress << sent;
            }
        });
        QCOMPARE(finish(reply.get()), Outcome::Ok);

        QCOMPARE(fake.requests.size(), 1);
        QCOMPARE(fake.requests.first().method, "PUT");
        QCOMPARE(fake.requests.first().headers.value("content-length"), QByteArray::number(contents.size()));
        QVERIFY(fake.requests.first().body == contents);
        // several steps, never going back, ending at the whole file
        QVERIFY2(progress.size() > 2, qPrintable(QString::number(progress.size())));
        QVERIFY(std::is_sorted(progress.cbegin(), progress.cend()));
        QCOMPARE(progress.last(), contents.size());
    }

    void encodedPath_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QByteArray>("path");
        QTest::newRow("space") << u"a b.txt"_s << QByteArray("/localapi/v0/file-put/nA/a%20b.txt");
        QTest::newRow("polish") << u"zażółć gęślą.txt"_s << QByteArray("/localapi/v0/file-put/nA/za%C5%BC%C3%B3%C5%82%C4%87%20g%C4%99%C5%9Bl%C4%85.txt");
        QTest::newRow("percent") << u"100%.txt"_s << QByteArray("/localapi/v0/file-put/nA/100%25.txt");
    }

    void encodedPath()
    {
        QFETCH(QString, fileName);
        QFETCH(QByteArray, path);
        FakeLocalApi fake;
        std::unique_ptr<QNetworkReply> reply(m_nam.put(LocalApi::request(LocalApi::filePutPath(u"nA"_s, fileName)), "x"));
        QCOMPARE(finish(reply.get()), Outcome::NodeNotFound);
        QCOMPARE(fake.requests.size(), 1);
        QCOMPARE(fake.requests.first().path, path);
        QCOMPARE(QUrl::fromPercentEncoding(path.mid(path.lastIndexOf('/') + 1)), fileName);
    }

    void outcome_data()
    {
        QTest::addColumn<int>("status");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<Outcome>("outcome");
        QTest::newRow("ok") << 200 << QByteArray() << Outcome::Ok;
        QTest::newRow("not operator") << 403 << QByteArray("file access denied\n") << Outcome::NotOperator;
        // passed on from the device
        QTest::newRow("no taildrop") << 403 << QByteArray("Taildrop disabled; no storage directory\n") << Outcome::DeviceRefused;
        QTest::newRow("not found") << 404 << QByteArray("node not found\n") << Outcome::NodeNotFound;
        QTest::newRow("bad gateway") << 502 << QByteArray() << Outcome::PeerUnreachable;
    }

    void outcome()
    {
        QFETCH(int, status);
        QFETCH(QByteArray, body);
        QFETCH(Outcome, outcome);
        FakeLocalApi fake;
        fake.respond("file-put/nA/a.txt", status, body);
        std::unique_ptr<QNetworkReply> reply(m_nam.put(LocalApi::request(LocalApi::filePutPath(u"nA"_s, u"a.txt"_s)), QByteArray()));
        QCOMPARE(finish(reply.get()), outcome);
        QCOMPARE(LocalApi::message(reply.get()), body.isEmpty() ? reply->errorString() : QString::fromUtf8(body).trimmed());
        QCOMPARE(reply->readAll(), body);
        QCOMPARE(fake.requests.first().headers.value("content-length"), "0");
    }

    // the body may come from the device and ends up in notifications
    void longMessage()
    {
        FakeLocalApi fake;
        fake.respond("file-put/nA/a.txt", 500, QByteArray(1024 * 1024, 'x'));
        std::unique_ptr<QNetworkReply> reply(m_nam.put(LocalApi::request(LocalApi::filePutPath(u"nA"_s, u"a.txt"_s)), QByteArray()));
        QCOMPARE(finish(reply.get()), Outcome::Other);
        QCOMPARE(LocalApi::message(reply.get()), QString(1024, u'x') + u'…');
    }

    void noDaemon()
    {
        QTemporaryDir dir;
        // nobody listening: a file where the socket should be, or nothing at all
        QFile notASocket(dir.filePath(u"refused.sock"_s));
        QVERIFY(notASocket.open(QIODevice::WriteOnly));
        for (const QString &path : {notASocket.fileName(), dir.filePath(u"missing.sock"_s)}) {
            qputenv("PLASMA_TAILDROP_SOCKET", path.toLocal8Bit());
            std::unique_ptr<QNetworkReply> reply(m_nam.get(LocalApi::request(u"file-targets"_s)));
            QCOMPARE(finish(reply.get()), Outcome::DaemonDown);
            QVERIFY(!LocalApi::message(reply.get()).isEmpty());
        }
        qunsetenv("PLASMA_TAILDROP_SOCKET");
    }

    // a transfer timeout is Cancelled too: whoever sets one and gets Cancelled without aborting has timed out
    void stall()
    {
        FakeLocalApi fake;
        fake.respond("file-targets", 0);
        QNetworkRequest request = LocalApi::request(u"file-targets"_s);
        request.setTransferTimeout(200);
        std::unique_ptr<QNetworkReply> reply(m_nam.get(request));
        QCOMPARE(finish(reply.get()), Outcome::Cancelled);
    }

    void abort()
    {
        FakeLocalApi fake;
        fake.respond("file-targets", 0);
        std::unique_ptr<QNetworkReply> reply(m_nam.get(LocalApi::request(u"file-targets"_s)));
        QTRY_COMPARE(fake.requests.size(), 1);
        reply->abort();
        QCOMPARE(finish(reply.get()), Outcome::Cancelled);
    }
};

QTEST_GUILESS_MAIN(LocalApiTest)

#include "localapitest.moc"
