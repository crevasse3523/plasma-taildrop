// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// The job of the built plugin: checks what was shared and starts the helper with it. A shell script stands in for
// the helper and writes down its arguments.

#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QDir>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <purpose/job.h>
#include <purpose/pluginbase.h>

#include <memory>
#include <unistd.h>

using namespace Qt::StringLiterals;

class PluginJobTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_arguments; // where the stub helper writes its arguments, one per line
    QString m_workingDirectory; // where the stub helper writes the directory it was started in
    std::unique_ptr<Purpose::PluginBase> m_plugin;

    QString makeFile(const QString &name)
    {
        QFile file(m_dir.filePath(name));
        return file.open(QIODevice::WriteOnly) && file.write("x") == 1 ? file.fileName() : QString();
    }

    // Runs the job to its end; returns its error text, empty on success
    QString run(const QJsonObject &data)
    {
        Purpose::Job *job = m_plugin->createJob();
        job->setData(data);
        QSignalSpy result(job, &KJob::result);
        job->start();
        // only later, so that whoever started it can connect first
        if (!result.isEmpty()) {
            return u"result from within start()"_s;
        }
        if (!result.wait()) {
            return u"no result"_s;
        }
        return job->error() ? job->errorText() : QString();
    }

    // What the helper was started with, once it wrote it down
    QStringList helperArguments() const
    {
        QFile file(m_arguments);
        if (!QTest::qWaitFor([&file] {
                return file.exists();
            })
            || !file.open(QIODevice::ReadOnly)) {
            return {u"helper not started"_s};
        }
        return QString::fromUtf8(file.readAll()).split(u'\n', Qt::SkipEmptyParts);
    }

    static QJsonObject share(const QString &device, const QStringList &paths)
    {
        QJsonArray urls;
        for (const QString &path : paths) {
            urls.append(path.contains(u':') ? path : QUrl::fromLocalFile(path).toString());
        }
        return {{u"device"_s, device}, {u"deviceName"_s, u"alpha"_s}, {u"urls"_s, urls}};
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        // the messages as written, whatever translation is installed
        KLocalizedString::setLanguages({u"en_US"_s});
        // the way Purpose loads it: PluginBase declares an interface its plugins do not, so qobject_cast finds none
        const auto result = KPluginFactory::instantiatePlugin<QObject>(KPluginMetaData(QStringLiteral(PLUGIN_PATH)));
        QVERIFY2(result, qPrintable(result.errorText));
        m_plugin.reset(dynamic_cast<Purpose::PluginBase *>(result.plugin));
        QVERIFY(m_plugin);

        m_arguments = m_dir.filePath(u"arguments"_s);
        m_workingDirectory = m_dir.filePath(u"working-directory"_s);
        QFile helper(m_dir.filePath(u"helper.sh"_s));
        QVERIFY(helper.open(QIODevice::WriteOnly));
        // written under another name first, so that the test never reads half of it
        helper.write("#!/bin/sh\npwd > \"" + QFile::encodeName(m_workingDirectory) + "\"\nprintf '%s\\n' \"$@\" > \"$0.tmp\" && mv \"$0.tmp\" \""
                     + QFile::encodeName(m_arguments) + "\"\n");
        helper.close();
        QVERIFY(helper.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        qputenv("PLASMA_TAILDROP_HELPER", QFile::encodeName(helper.fileName()));
    }

    void cleanup()
    {
        QFile::remove(m_arguments);
        QFile::remove(m_workingDirectory);
    }

    void files()
    {
        const QString first = makeFile(u"zażółć gęślą.txt"_s);
        const QString second = makeFile(u"b.txt"_s);
        QCOMPARE(run(share(u"nA"_s, {first, second, first})), QString());
        // each file once
        QCOMPARE(helperArguments(), QStringList({u"--device-id=nA"_s, u"--device-name=alpha"_s, u"--"_s, first, second}));
    }

    // QGuiApplication takes options such as -platform from anywhere in its argv, even after --
    void optionLikeValues()
    {
        const QString file = makeFile(u"a.txt"_s);
        QJsonObject data = share(u"-platform"_s, {file});
        data[u"deviceName"_s] = u"-session"_s;
        QCOMPARE(run(data), QString());
        QCOMPARE(helperArguments(), QStringList({u"--device-id=-platform"_s, u"--device-name=-session"_s, u"--"_s, file}));
    }

    // the helper outlives the sharing application and must not keep its directory, e.g. on a USB stick, busy
    void workingDirectory()
    {
        const QString previous = QDir::currentPath();
        QVERIFY(QDir::setCurrent(m_dir.path()));
        const QString error = run(share(u"nA"_s, {makeFile(u"a.txt"_s)}));
        QVERIFY(QDir::setCurrent(previous));
        QCOMPARE(error, QString());
        QVERIFY(helperArguments().contains(u"--"_s));
        QFile file(m_workingDirectory);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll().trimmed(), QByteArray("/"));
    }

    // more paths than execve takes; the reason must not read like a broken installation
    void tooManyFiles()
    {
        QString directory = m_dir.filePath(u"many"_s);
        for (int level = 0; level < 8; ++level) {
            directory += u'/' + QString(200, u'd');
        }
        QVERIFY(QDir().mkpath(directory));
        QStringList paths;
        qint64 size = 0;
        // execve caps arguments at 6 MiB (3/4 of _STK_LIM) whatever the stack limit
        for (int i = 0; size <= 6 * 1024 * 1024; ++i) {
            const QString path = directory + u'/' + QString::number(i) + QString(200, u'f');
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            paths.append(path);
            size += path.size();
        }
        const QString helper = qEnvironmentVariable("PLASMA_TAILDROP_HELPER");
        const QString error = run(share(u"nA"_s, paths));
        QVERIFY2(error.startsWith(u"Could not start "_s + helper + u": "_s), qPrintable(error));
        QVERIFY(error.size() > helper.size() + 18);
        QVERIFY(QDir(m_dir.filePath(u"many"_s)).removeRecursively());
    }

    void refused_data()
    {
        QTest::addColumn<QJsonObject>("data");
        QTest::addColumn<QString>("error");
        const QString file = makeFile(u"a.txt"_s);
        QTest::newRow("no device") << share({}, {file}) << u"No device chosen"_s;
        QTest::newRow("nothing") << share(u"nA"_s, {}) << u"No files to send"_s;
        QTest::newRow("remote") << share(u"nA"_s, {u"https://example.org/a.txt"_s}) << u"Only local files can be sent: https://example.org/a.txt"_s;
        QTest::newRow("missing") << share(u"nA"_s, {file, m_dir.filePath(u"gone.txt"_s)}) << m_dir.filePath(u"gone.txt"_s) + u" does not exist"_s;
        QTest::newRow("folder") << share(u"nA"_s, {file, m_dir.path()}) << u"Only files can be sent, not folders: "_s + m_dir.path();
    }

    void refused()
    {
        QFETCH(QJsonObject, data);
        QFETCH(QString, error);
        QCOMPARE(run(data), error);
        QTest::qWait(100);
        QVERIFY(!QFile::exists(m_arguments));
    }

    void unreadable()
    {
        if (::geteuid() == 0) {
            QSKIP("root can read everything");
        }
        const QString file = makeFile(u"secret.txt"_s);
        QVERIFY(QFile::setPermissions(file, QFile::WriteOwner));
        QCOMPARE(run(share(u"nA"_s, {file})), u"Could not read "_s + file);
    }

    void noHelper()
    {
        const QByteArray helper = qgetenv("PLASMA_TAILDROP_HELPER");
        qputenv("PLASMA_TAILDROP_HELPER", QFile::encodeName(m_dir.filePath(u"missing-helper"_s)));
        const QString error = run(share(u"nA"_s, {makeFile(u"a.txt"_s)}));
        qputenv("PLASMA_TAILDROP_HELPER", helper);
        QVERIFY2(error.startsWith(u"Could not start "_s + m_dir.filePath(u"missing-helper"_s) + u": "_s), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(PluginJobTest)

#include "pluginjobtest.moc"
