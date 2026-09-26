// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "targets.h"
#include "localapi.h"

#include <QFile>
#include <QTest>

using namespace Qt::StringLiterals;

class TargetsTest : public QObject
{
    Q_OBJECT

    QByteArray m_json;

    static QStringList names(const QList<Target> &targets)
    {
        QStringList result;
        for (const Target &target : targets) {
            result << target.name;
        }
        return result;
    }

    static Target find(const QList<Target> &targets, const QString &name)
    {
        const auto it = std::find_if(targets.cbegin(), targets.cend(), [&name](const Target &target) {
            return target.name == name;
        });
        return it == targets.cend() ? Target{} : *it;
    }

private Q_SLOTS:
    void initTestCase()
    {
        // anonymized reply of GET /localapi/v0/file-targets
        QFile file(QFINDTESTDATA("data/file-targets.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        m_json = file.readAll();
    }

    void parsesFields()
    {
        const auto targets = parseTargetsJson(m_json);
        QCOMPARE(names(targets), (QStringList{u"Beta"_s, u"gamma"_s, u"alpha"_s, u"delta"_s, u"epsilon"_s, u"zeta"_s}));
        const Target beta = find(targets, u"Beta"_s);
        QCOMPARE(beta.stableId, u"nBetaStable2CNTRL"_s);
        QCOMPARE(beta.name, u"Beta"_s);
        QCOMPARE(beta.ip, u"100.64.0.2"_s);
        QCOMPARE(beta.os, u"windows"_s);
        QVERIFY(beta.online);
        QVERIFY(!beta.lastSeen.isValid());

        const Target zeta = find(targets, u"zeta"_s);
        QVERIFY(!zeta.online);
        QCOMPARE(zeta.lastSeen, QDateTime(QDate(2026, 9, 12), QTime(17, 23, 59, 100), QTimeZone::UTC));
    }

    void onlyIpv6Address()
    {
        QCOMPARE(find(parseTargetsJson(m_json), u"gamma"_s).ip, u"fd7a:115c:a1e0::4"_s);
    }

    void missingOnlineMeansOffline()
    {
        const Target epsilon = find(parseTargetsJson(m_json), u"epsilon"_s);
        QCOMPARE(epsilon.stableId, u"nUnknownStat6CNTRL"_s);
        QVERIFY(!epsilon.online);
        QVERIFY(!epsilon.lastSeen.isValid());
    }

    void skipsIncompleteEntries()
    {
        // raw string literals confuse moc
        const QByteArray json =
            "[{\"Node\": {\"StableID\": \"nA\", \"ComputedName\": \"a\"}},"
            " {\"Node\": {\"StableID\": \"\", \"ComputedName\": \"no-id\"}},"
            " {\"Node\": {\"StableID\": \"nB\"}},"
            " {\"PeerAPIURL\": \"http://100.64.0.9:1\"},"
            " 42]";
        const auto targets = parseTargetsJson(json);
        QCOMPARE(names(targets), QStringList{u"a"_s});
        QCOMPARE(targets[0].ip, QString());
        QCOMPARE(targets[0].os, QString());
        QVERIFY(!targets[0].online);
    }

    void malformedJson_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("null") << QByteArray("null");
        QTest::newRow("object") << QByteArray("{\"Node\": {\"StableID\": \"nA\", \"ComputedName\": \"a\"}}");
        QTest::newRow("truncated") << m_json.left(m_json.size() / 2);
    }

    void malformedJson()
    {
        QFETCH(QByteArray, json);
        QVERIFY(parseTargetsJson(json).isEmpty());
    }

    void connectionPaths()
    {
        QFile file(QFINDTESTDATA("data/status.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto targets = parseTargetsJson(m_json);
        readConnectionPaths(file.readAll(), targets);
        QCOMPARE(find(targets, u"Beta"_s).directAddress, u"192.168.1.20"_s);
        QCOMPARE(find(targets, u"Beta"_s).relay, u"waw"_s);
        QCOMPARE(find(targets, u"gamma"_s).directAddress, u"2001:db8::7"_s);
        QCOMPARE(find(targets, u"gamma"_s).peerRelay, QString());
        QCOMPARE(find(targets, u"alpha"_s).directAddress, QString());
        QCOMPARE(find(targets, u"alpha"_s).peerRelay, u"198.51.100.7"_s);
        QCOMPARE(find(targets, u"delta"_s).directAddress, QString());
        QCOMPARE(find(targets, u"delta"_s).relay, u"waw"_s);
        QCOMPARE(find(targets, u"epsilon"_s).relay, QString());
        // not in the status reply
        QCOMPARE(find(targets, u"zeta"_s).relay, QString());
        QCOMPARE(targets.size(), 6);
    }

    void connectionPathsMalformed()
    {
        auto targets = parseTargetsJson(m_json);
        readConnectionPaths("{\"Peer\": [1, 2]}", targets);
        readConnectionPaths("not json", targets);
        QVERIFY(std::all_of(targets.cbegin(), targets.cend(), [](const Target &target) {
            return target.directAddress.isEmpty() && target.peerRelay.isEmpty() && target.relay.isEmpty();
        }));
    }

    void filePutPath_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QString>("path");
        QTest::newRow("plain") << u"photo.jpg"_s << u"file-put/nA/photo.jpg"_s;
        QTest::newRow("space") << u"a b.txt"_s << u"file-put/nA/a%20b.txt"_s;
        QTest::newRow("polish") << u"zażółć gęślą.txt"_s << u"file-put/nA/za%C5%BC%C3%B3%C5%82%C4%87%20g%C4%99%C5%9Bl%C4%85.txt"_s;
        QTest::newRow("percent") << u"100%.txt"_s << u"file-put/nA/100%25.txt"_s;
        QTest::newRow("reserved") << u"a/b?c#d.txt"_s << u"file-put/nA/a%2Fb%3Fc%23d.txt"_s;
    }

    void filePutPath()
    {
        QFETCH(QString, fileName);
        QFETCH(QString, path);
        QCOMPARE(LocalApi::filePutPath(u"nA"_s, fileName), path);
        // the encoding has to survive QUrl, which sends it
        QCOMPARE(LocalApi::request(path).url().path(QUrl::FullyEncoded), u"/localapi/v0/"_s + path);
    }

    void request()
    {
        qputenv("PLASMA_TAILDROP_SOCKET", "/tmp/fake.sock");
        const QNetworkRequest request = LocalApi::request(u"file-targets"_s);
        qunsetenv("PLASMA_TAILDROP_SOCKET");
        QCOMPARE(request.url().toString(), u"unix+http://local-tailscaled.sock/localapi/v0/file-targets"_s);
        QCOMPARE(request.attribute(QNetworkRequest::FullLocalServerNameAttribute).toString(), u"/tmp/fake.sock"_s);
    }

    void socketPath()
    {
        qputenv("PLASMA_TAILDROP_SOCKET", "/tmp/fake.sock");
        QCOMPARE(LocalApi::socketPath(), u"/tmp/fake.sock"_s);
        qunsetenv("PLASMA_TAILDROP_SOCKET");
        QVERIFY(LocalApi::socketPath().endsWith(u"/tailscale/tailscaled.sock"_s));
    }

    void classify_data()
    {
        using LocalApi::Outcome;
        QTest::addColumn<int>("status");
        QTest::addColumn<QNetworkReply::NetworkError>("error");
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<Outcome>("outcome");
        QTest::newRow("ok") << 200 << QNetworkReply::NoError << QByteArray() << Outcome::Ok;
        QTest::newRow("not operator") << 403 << QNetworkReply::ContentAccessDenied << QByteArray("file access denied\n") << Outcome::NotOperator;
        // the device's refusal, passed on by tailscaled
        QTest::newRow("no taildrop") << 403 << QNetworkReply::ContentAccessDenied << QByteArray("Taildrop disabled; no storage directory\n")
                                     << Outcome::DeviceRefused;
        QTest::newRow("not found") << 404 << QNetworkReply::ContentNotFoundError << QByteArray() << Outcome::NodeNotFound;
        QTest::newRow("bad gateway") << 502 << QNetworkReply::UnknownServerError << QByteArray() << Outcome::PeerUnreachable;
        QTest::newRow("gateway timeout") << 504 << QNetworkReply::UnknownServerError << QByteArray() << Outcome::PeerUnreachable;
        // e.g. the device could not store the file
        QTest::newRow("server error") << 500 << QNetworkReply::InternalServerError << QByteArray("disk full\n") << Outcome::Other;
        QTest::newRow("bad request") << 400 << QNetworkReply::ProtocolInvalidOperationError << QByteArray() << Outcome::Other;
        QTest::newRow("refused") << 0 << QNetworkReply::ConnectionRefusedError << QByteArray() << Outcome::DaemonDown;
        QTest::newRow("no socket") << 0 << QNetworkReply::HostNotFoundError << QByteArray() << Outcome::DaemonDown;
        QTest::newRow("timeout") << 0 << QNetworkReply::TimeoutError << QByteArray() << Outcome::PeerUnreachable;
        QTest::newRow("aborted") << 0 << QNetworkReply::OperationCanceledError << QByteArray() << Outcome::Cancelled;
        QTest::newRow("aborted after headers") << 200 << QNetworkReply::OperationCanceledError << QByteArray() << Outcome::Cancelled;
        QTest::newRow("closed") << 0 << QNetworkReply::RemoteHostClosedError << QByteArray() << Outcome::Other;
    }

    void classify()
    {
        QFETCH(int, status);
        QFETCH(QNetworkReply::NetworkError, error);
        QFETCH(QByteArray, body);
        QFETCH(LocalApi::Outcome, outcome);
        QCOMPARE(LocalApi::classify(status, error, body), outcome);
    }
};

QTEST_GUILESS_MAIN(TargetsTest)

#include "targetstest.moc"
