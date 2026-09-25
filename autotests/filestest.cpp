// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "files.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

class FilesTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString path(const QString &name) const
    {
        return m_dir.filePath(name);
    }

    // as Purpose passes them
    QString url(const QString &name) const
    {
        return QUrl::fromLocalFile(path(name)).toString();
    }

    void write(const QString &name, const QByteArray &contents)
    {
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(contents), contents.size());
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        write(u"a.txt"_s, "abc");
        write(u"-rf.txt"_s, "12345");
        write(u"zażółć gęślą.txt"_s, "");
        QVERIFY(QDir(m_dir.path()).mkpath(u"folder/inner"_s));
    }

    void empty()
    {
        const ValidatedUrls result = validateUrls({});
        QVERIFY(result.files.isEmpty());
        QVERIFY(result.folders.isEmpty());
        QVERIFY(result.problems.isEmpty());
        QCOMPARE(result.totalBytes, 0);
    }

    void filesAndTotals()
    {
        const ValidatedUrls result = validateUrls({url(u"a.txt"_s), url(u"-rf.txt"_s), url(u"zażółć gęślą.txt"_s)});
        QCOMPARE(result.files, (QStringList{path(u"a.txt"_s), path(u"-rf.txt"_s), path(u"zażółć gęślą.txt"_s)}));
        QCOMPARE(result.totalBytes, 8);
        QVERIFY(result.folders.isEmpty());
        QVERIFY(result.problems.isEmpty());
    }

    void folders()
    {
        const ValidatedUrls result = validateUrls({url(u"folder"_s), url(u"a.txt"_s)});
        QCOMPARE(result.folders, QStringList{path(u"folder"_s)});
        QCOMPARE(result.files, QStringList{path(u"a.txt"_s)});
        QCOMPARE(result.totalBytes, 3);
    }

    void duplicates()
    {
        const ValidatedUrls result = validateUrls({url(u"a.txt"_s), url(u"folder/../a.txt"_s), url(u"./a.txt"_s), url(u"folder"_s), url(u"folder/"_s)});
        QCOMPARE(result.files, QStringList{path(u"a.txt"_s)});
        QCOMPARE(result.folders, QStringList{path(u"folder"_s)});
        QCOMPARE(result.totalBytes, 3);
    }

    void problems()
    {
        const QString remote = u"https://example.com/a.txt"_s;
        const QString missing = url(u"missing.txt"_s);
        const ValidatedUrls result = validateUrls({remote, missing, url(u"a.txt"_s), QString()});
        QCOMPARE(result.files, QStringList{path(u"a.txt"_s)});
        QCOMPARE(result.problems.size(), 3);
        QCOMPARE(result.problems[0].reason, UrlProblem::NotLocal);
        QCOMPARE(result.problems[0].url, QUrl(remote));
        QCOMPARE(result.problems[1].reason, UrlProblem::Missing);
        QCOMPARE(result.problems[1].url, QUrl(missing));
        QCOMPARE(result.problems[2].reason, UrlProblem::NotLocal);
    }

    void unreadable()
    {
        if (geteuid() == 0) {
            QSKIP("root can read everything");
        }
        write(u"secret.txt"_s, "x");
        QVERIFY(QFile::setPermissions(path(u"secret.txt"_s), {}));
        const ValidatedUrls result = validateUrls({url(u"secret.txt"_s)});
        QVERIFY(result.files.isEmpty());
        QCOMPARE(result.problems.size(), 1);
        QCOMPARE(result.problems[0].reason, UrlProblem::Unreadable);
    }

    void unsupported()
    {
        QCOMPARE(mkfifo(QFile::encodeName(path(u"fifo"_s)).constData(), 0600), 0);
        const ValidatedUrls result = validateUrls({url(u"fifo"_s)});
        QVERIFY(result.files.isEmpty());
        QCOMPARE(result.problems.size(), 1);
        QCOMPARE(result.problems[0].reason, UrlProblem::Unsupported);
    }
};

QTEST_GUILESS_MAIN(FilesTest)

#include "filestest.moc"
