// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// The device list of the Share dialog, loaded from FakeLocalApi

#include "fakelocalapi.h"
#include "settings.h"
#include "tailscaletargets.h"

#include <KLocalizedString>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::StringLiterals;

class TargetsModelTest : public QObject
{
    Q_OBJECT

    QByteArray m_targetsJson;
    QByteArray m_statusJson;

    static QByteArray read(const QString &name)
    {
        QFile file(QFINDTESTDATA(u"data/"_s + name));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    // A fake tailscaled with the fixtures, for a user allowed to send
    std::unique_ptr<FakeLocalApi> tailscaled() const
    {
        auto fake = std::make_unique<FakeLocalApi>();
        fake->respond("file-targets", 200, m_targetsJson);
        fake->respond("status", 200, m_statusJson);
        fake->respond("file-put/plasma-taildrop-probe/probe", 404, "unknown peer\n");
        return fake;
    }

    static bool waitLoaded(TailscaleTargetsModel &model)
    {
        QSignalSpy loaded(&model, &TailscaleTargetsModel::loaded);
        return loaded.wait(5000);
    }

    static QVariant value(const TailscaleTargetsModel &model, const QString &stableId, TailscaleTargetsModel::Roles role)
    {
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row);
            if (index.data(TailscaleTargetsModel::StableIdRole) == stableId) {
                return index.data(role);
            }
        }
        return {};
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setLanguages({u"en_US"_s});
        m_targetsJson = read(u"file-targets.json"_s);
        m_statusJson = read(u"status.json"_s);
        QVERIFY(!m_targetsJson.isEmpty() && !m_statusJson.isEmpty());
    }

    void init()
    {
        QSettings(Settings::Name).clear();
    }

    void lists()
    {
        const auto fake = tailscaled();
        TailscaleTargetsModel model;
        QVERIFY(model.loading());
        QVERIFY(waitLoaded(model));
        QVERIFY(!model.loading());
        QCOMPARE(model.error(), QString());
        QVERIFY(model.canSend());
        QCOMPARE(model.permissionHint(), QString());
        QCOMPARE(model.rowCount(), 6);
        QCOMPARE(model.index(0).data(TailscaleTargetsModel::NameRole), u"Beta"_s);
        QCOMPARE(model.preselected(), QString());

        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::IpRole), u"100.64.0.2"_s);
        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::OsRole), u"windows"_s);
        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::StatusTextRole), QString());
        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::PathRole), u"direct (192.168.1.20)"_s);
        QCOMPARE(value(model, u"nGammaStable4CNTRL"_s, TailscaleTargetsModel::PathRole), u"direct (2001:db8::7)"_s);
        // offline: no path, when it was last seen if known
        QCOMPARE(value(model, u"nDeltaStable5CNTRL"_s, TailscaleTargetsModel::PathRole), QString());
        QVERIFY(value(model, u"nDeltaStable5CNTRL"_s, TailscaleTargetsModel::StatusTextRole).toString().startsWith(u"offline, last seen "_s));
        QCOMPARE(value(model, u"nUnknownStat6CNTRL"_s, TailscaleTargetsModel::StatusTextRole), u"offline"_s);

        // the probe sends nothing
        const auto probe = std::find_if(fake->requests.cbegin(), fake->requests.cend(), [](const FakeLocalApi::Request &request) {
            return request.method == "PUT";
        });
        QVERIFY(probe != fake->requests.cend());
        QCOMPARE(probe->path, "/localapi/v0/file-put/plasma-taildrop-probe/probe");
        QCOMPARE(probe->body, QByteArray());
        QCOMPARE(fake->requests.size(), 3);
    }

    void relayed()
    {
        const auto fake = tailscaled();
        fake->respond("status", 200, QByteArray(m_statusJson).replace("192.168.1.20:41641", ""));
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::PathRole), u"via DERP relay (waw)"_s);
    }

    void peerRelayed()
    {
        const auto fake = tailscaled();
        fake->respond("status",
                      200,
                      QByteArray(m_statusJson).replace("192.168.1.20:41641", "").replace("\"PeerRelay\": \"\"", "\"PeerRelay\": \"[2001:db8::9]:7777:vni:5\""));
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::PathRole), u"via peer relay (2001:db8::9)"_s);
    }

    void withoutStatus()
    {
        const auto fake = tailscaled();
        fake->respond("status", 500);
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QCOMPARE(model.error(), QString());
        QCOMPARE(model.rowCount(), 6);
        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::PathRole), QString());
    }

    // tailscaled went away in the middle of the list
    void cutOff()
    {
        const auto fake = tailscaled();
        fake->respondRaw("file-targets", "HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n[{");
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QCOMPARE(model.rowCount(), 0);
        QVERIFY(model.error().startsWith(u"Could not list the devices: "_s));
        QVERIFY(!model.error().contains(u"[{"_s));
    }

    void notOperator()
    {
        const auto fake = tailscaled();
        fake->respond("file-put/plasma-taildrop-probe/probe", 403, "file access denied\n");
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QCOMPARE(model.rowCount(), 6);
        QVERIFY(!model.canSend());
        QVERIFY(model.permissionHint().contains(u"sudo tailscale set --operator=$USER"_s));
    }

    void daemonDown()
    {
        QTemporaryDir dir;
        qputenv("PLASMA_TAILDROP_SOCKET", dir.filePath(u"missing.sock"_s).toLocal8Bit());
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        qunsetenv("PLASMA_TAILDROP_SOCKET");
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.error(), u"Tailscale is not running."_s);
        QVERIFY(!model.canSend());
        // the error says it all
        QCOMPARE(model.permissionHint(), QString());
    }

    void preselectsLastUsed()
    {
        QSettings(Settings::Name).setValue(Settings::LastTargetIdKey, u"nGammaStable4CNTRL"_s);
        const auto fake = tailscaled();
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QCOMPARE(model.preselected(), u"nGammaStable4CNTRL"_s);
    }

    void noPreselectionWhenOffline()
    {
        QSettings(Settings::Name).setValue(Settings::LastTargetIdKey, u"nDeltaStable5CNTRL"_s);
        const auto fake = tailscaled();
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QCOMPARE(model.index(0).data(TailscaleTargetsModel::StableIdRole), u"nDeltaStable5CNTRL"_s);
        QCOMPARE(model.preselected(), QString());
    }

    void isOnline()
    {
        const auto fake = tailscaled();
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QVERIFY(model.isOnline(u"nBetaStable2CNTRL"_s));
        QVERIFY(!model.isOnline(u"nDeltaStable5CNTRL"_s));
        QVERIFY(!model.isOnline(u"nGoneCNTRL"_s));
        QVERIFY(!model.isOnline(QString()));
    }

    void nameOf()
    {
        const auto fake = tailscaled();
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        QCOMPARE(model.nameOf(u"nBetaStable2CNTRL"_s), u"Beta"_s);
        QCOMPARE(model.nameOf(u"nGoneCNTRL"_s), QString());
    }

    void reload()
    {
        const auto fake = tailscaled();
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        fake->respond("file-targets", 200, "[]");
        QSignalSpy loadingChanged(&model, &TailscaleTargetsModel::loadingChanged);
        model.reload();
        QVERIFY(model.loading());
        QCOMPARE(model.rowCount(), 6);
        QVERIFY(waitLoaded(model));
        QCOMPARE(loadingChanged.size(), 2);
        QCOMPARE(model.rowCount(), 0);
    }

    void reloadWhileLoading()
    {
        const auto fake = tailscaled();
        fake->respond("file-targets", 0);
        TailscaleTargetsModel model;
        QSignalSpy loaded(&model, &TailscaleTargetsModel::loaded);
        QTRY_COMPARE(fake->requests.size(), 3);
        fake->respond("file-targets", 200, m_targetsJson);
        model.reload();
        QVERIFY(loaded.wait(5000));
        QCOMPARE(model.rowCount(), 6);
        // the stalled request never reports
        QTest::qWait(100);
        QCOMPARE(loaded.size(), 1);
    }
};

QTEST_GUILESS_MAIN(TargetsModelTest)

#include "targetsmodeltest.moc"
