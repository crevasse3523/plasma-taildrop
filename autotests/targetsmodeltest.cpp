// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// The device list of the Share dialog, loaded from FakeLocalApi

#include "fakelocalapi.h"
#include "tailscaletargets.h"

#include <KLocalizedString>
#include <QFile>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TargetsModelTest : public QObject
{
    Q_OBJECT

    QByteArray m_targetsJson;

    static QByteArray read(const QString &name)
    {
        QFile file(QFINDTESTDATA(u"data/"_s + name));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    // A fake tailscaled with the fixtures
    std::unique_ptr<FakeLocalApi> tailscaled() const
    {
        auto fake = std::make_unique<FakeLocalApi>();
        fake->respond("file-targets", 200, m_targetsJson);
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
        KLocalizedString::setLanguages({u"en_US"_s});
        m_targetsJson = read(u"file-targets.json"_s);
        QVERIFY(!m_targetsJson.isEmpty());
    }

    void lists()
    {
        const auto fake = tailscaled();
        TailscaleTargetsModel model;
        QVERIFY(model.loading());
        QVERIFY(waitLoaded(model));
        QVERIFY(!model.loading());
        QCOMPARE(model.error(), QString());
        QCOMPARE(model.rowCount(), 6);
        QCOMPARE(model.index(0).data(TailscaleTargetsModel::NameRole), u"Beta"_s);

        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::IpRole), u"100.64.0.2"_s);
        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::OsRole), u"windows"_s);
        QCOMPARE(value(model, u"nBetaStable2CNTRL"_s, TailscaleTargetsModel::OnlineRole), true);
        QCOMPARE(value(model, u"nDeltaStable5CNTRL"_s, TailscaleTargetsModel::OnlineRole), false);
        QCOMPARE(fake->requests.size(), 1);
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

    void daemonDown()
    {
        QTemporaryDir dir;
        qputenv("PLASMA_TAILDROP_SOCKET", dir.filePath(u"missing.sock"_s).toLocal8Bit());
        TailscaleTargetsModel model;
        QVERIFY(waitLoaded(model));
        qunsetenv("PLASMA_TAILDROP_SOCKET");
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.error(), u"Tailscale is not running."_s);
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
        QTRY_COMPARE(fake->requests.size(), 1);
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
