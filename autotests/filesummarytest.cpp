// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// The heading of the Share dialog

#include "filesummary.h"

#include <KLocalizedString>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class FileSummaryTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString url(const QString &name) const
    {
        return QUrl::fromLocalFile(m_dir.filePath(name)).toString();
    }

private Q_SLOTS:
    void initTestCase()
    {
        KLocalizedString::setLanguages({u"en_US"_s});
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates)); // for the sizes
        QVERIFY(m_dir.isValid());
        for (const auto &[name, size] : {std::pair{u"a.txt"_s, 100}, std::pair{u"b.txt"_s, 1948}}) {
            QFile file(m_dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(QByteArray(size, 'x')), size);
        }
        QVERIFY(QDir(m_dir.path()).mkdir(u"folder"_s));
    }

    void summarize_data()
    {
        QTest::addColumn<QStringList>("urls");
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("hasFolders");
        QTest::addColumn<bool>("problem");

        QTest::newRow("one file") << QStringList{url(u"a.txt"_s)} << u"Send 1 file (100 B) to:"_s << false << false;
        QTest::newRow("files") << QStringList{url(u"a.txt"_s), url(u"b.txt"_s), url(u"a.txt"_s)} << u"Send 2 files (2.0 KiB) to:"_s << false << false;
        QTest::newRow("folder") << QStringList{url(u"folder"_s)} << u"Send 1 folder to:"_s << true << false;
        QTest::newRow("both") << QStringList{url(u"a.txt"_s), url(u"folder"_s)} << u"Send 1 file (100 B) and 1 folder to:"_s << true << false;
        QTest::newRow("missing") << QStringList{url(u"a.txt"_s), url(u"gone.txt"_s)} << u"Send 1 file (100 B) to:"_s << false << true;
        QTest::newRow("remote") << QStringList{u"https://example.org/a.txt"_s} << u"Choose a device to send to:"_s << false << true;
    }

    void summarize()
    {
        QFETCH(QStringList, urls);
        const QVariantMap summary = FileSummary().summarize(urls);
        QTEST(summary.value(u"text"_s).toString(), "text");
        QTEST(summary.value(u"hasFolders"_s).toBool(), "hasFolders");
        QTEST(!summary.value(u"problem"_s).toString().isEmpty(), "problem");
    }
};

QTEST_GUILESS_MAIN(FileSummaryTest)

#include "filesummarytest.moc"
