// SPDX-FileCopyrightText: 2026 crevasse3523 <335460626+crevasse3523@users.noreply.github.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// Packs a folder in every format, opens the archive again and compares it with the folder

#include "archiver.h"

#include <KArchiveDirectory>
#include <KArchiveFile>
#include <KTar>
#include <KZip>
#include <QDir>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>

#include <fcntl.h>

#include <sys/stat.h>
#include <unistd.h>

using namespace Qt::StringLiterals;

class ArchiverTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_folder;
    QString m_slow; // takes seconds to pack as tar.xz
    QByteArray m_big;

    static void writeFile(const QString &path, const QByteArray &contents, QIODevice::OpenMode mode = QIODevice::WriteOnly)
    {
        QFile file(path);
        QVERIFY(file.open(mode));
        QCOMPARE(file.write(contents), contents.size());
    }

    void write(const QString &name, const QByteArray &contents)
    {
        writeFile(m_folder + u'/' + name, contents);
    }

    // not compressible, so it takes a while to pack
    static QByteArray randomBytes(qsizetype size)
    {
        QByteArray bytes(size, Qt::Uninitialized);
        QRandomGenerator(42).fillRange(reinterpret_cast<quint32 *>(bytes.data()), bytes.size() / sizeof(quint32));
        return bytes;
    }

    static QString cache()
    {
        return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }

    static QStringList archivesLeft()
    {
        return QDir(cache()).entryList(QDir::Files);
    }

    static std::unique_ptr<KArchive> open(const QString &path)
    {
        std::unique_ptr<KArchive> archive;
        if (path.endsWith(u".zip"_s)) {
            archive = std::make_unique<KZip>(path);
        } else {
            archive = std::make_unique<KTar>(path);
        }
        return archive->open(QIODevice::ReadOnly) ? std::move(archive) : nullptr;
    }

    // "name" for files, "name/" for folders and "name -> target" for links, of everything in the archive
    static QStringList list(const KArchiveDirectory *directory, const QString &prefix = {})
    {
        QStringList entries;
        for (const QString &name : directory->entries()) {
            const KArchiveEntry *entry = directory->entry(name);
            const QString path = prefix + name;
            if (!entry->symLinkTarget().isEmpty()) {
                entries.append(path + u" -> "_s + entry->symLinkTarget());
            } else if (entry->isDirectory()) {
                entries.append(path + u'/');
                entries += list(static_cast<const KArchiveDirectory *>(entry), path + u'/');
            } else {
                entries.append(path);
            }
        }
        entries.sort();
        return entries;
    }

    static QByteArray contents(const KArchive &archive, const QString &path)
    {
        const KArchiveFile *file = archive.directory()->file(path);
        return file ? file->data() : "<missing>";
    }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        // the cache goes under $HOME in test mode, and packHome() shares that folder
        qputenv("HOME", QFile::encodeName(m_dir.filePath(u"dom"_s)));
        QStandardPaths::setTestModeEnabled(true);
        m_folder = m_dir.filePath(u"Zdjęcia z wakacji"_s);
        QVERIFY(QDir().mkpath(m_folder + u"/pusty folder"_s));
        QVERIFY(QDir().mkpath(m_folder + u"/dzień 1/.ukryte"_s));
        write(u"żółw.txt"_s, "turtle\n");
        write(u"empty"_s, {});
        write(u"dzień 1/.ukryte/notatka.md"_s, "# notatka\n");
        // bigger than a read chunk, and not compressible, so it is packed in several steps
        m_big = randomBytes(3 * 1024 * 1024 + 17);
        write(u"dzień 1/big.bin"_s, m_big);
        QVERIFY(QFile::link(u"../żółw.txt"_s, m_folder + u"/dzień 1/link do żółwia"_s));
        QVERIFY(::mkfifo(QFile::encodeName(m_folder + u"/fifo"_s).constData(), 0600) == 0);
        m_slow = m_dir.filePath(u"wolny"_s);
        QVERIFY(QDir().mkpath(m_slow));
        writeFile(m_slow + u"/big.bin"_s, randomBytes(32 * 1024 * 1024));
    }

    void formats()
    {
        const QStringList formats = Archiver::formats();
        QCOMPARE(formats.mid(0, 3), (QStringList{u"zip"_s, u"tar.gz"_s, u"tar.xz"_s}));
        // Debian's KArchive is built with zstd
        QVERIFY(formats.contains(u"tar.zst"_s));
    }

    void pack_data()
    {
        QTest::addColumn<QString>("format");
        for (const QString &format : Archiver::formats()) {
            QTest::newRow(qPrintable(format)) << format;
        }
    }

    void pack()
    {
        QFETCH(QString, format);
        Archiver archiver;
        QSignalSpy progress(&archiver, &Packer::progress);
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(m_folder, format);
        QVERIFY(finished.wait(30000));
        const QString path = finished.first().at(0).toString();
        QCOMPARE(finished.first().at(1).toString(), QString());
        QVERIFY(path.startsWith(cache() + u'/'));
        QVERIFY(path.endsWith(u'.' + format));

        const qint64 total = m_big.size() + 7 + 10;
        QVERIFY(progress.size() > 2);
        QCOMPARE(progress.last(), (QVariantList{total, total}));
        for (qsizetype i = 1; i < progress.size(); ++i) {
            QVERIFY(progress.at(i).first().toLongLong() > progress.at(i - 1).first().toLongLong());
        }

        const std::unique_ptr<KArchive> archive = open(path);
        QVERIFY(archive);
        QCOMPARE(list(archive->directory()),
                 (QStringList{
                     u"Zdjęcia z wakacji/"_s,
                     u"Zdjęcia z wakacji/dzień 1/"_s,
                     u"Zdjęcia z wakacji/dzień 1/.ukryte/"_s,
                     u"Zdjęcia z wakacji/dzień 1/.ukryte/notatka.md"_s,
                     u"Zdjęcia z wakacji/dzień 1/big.bin"_s,
                     u"Zdjęcia z wakacji/dzień 1/link do żółwia -> ../żółw.txt"_s,
                     u"Zdjęcia z wakacji/empty"_s,
                     u"Zdjęcia z wakacji/pusty folder/"_s,
                     u"Zdjęcia z wakacji/żółw.txt"_s,
                 }));
        QCOMPARE(contents(*archive, u"Zdjęcia z wakacji/żółw.txt"_s), "turtle\n");
        QCOMPARE(contents(*archive, u"Zdjęcia z wakacji/empty"_s), QByteArray());
        QCOMPARE(contents(*archive, u"Zdjęcia z wakacji/dzień 1/.ukryte/notatka.md"_s), "# notatka\n");
        QVERIFY(contents(*archive, u"Zdjęcia z wakacji/dzień 1/big.bin"_s) == m_big);
        QVERIFY(QFile::remove(path));
    }

    void packSymlinkedFolder()
    {
        const QString link = m_dir.filePath(u"skrót"_s);
        QVERIFY(QFile::link(m_folder + u"/dzień 1/.ukryte"_s, link));
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(link, u"zip"_s);
        QVERIFY(finished.wait(30000));
        const QString path = finished.first().at(0).toString();
        const std::unique_ptr<KArchive> archive = open(path);
        QVERIFY(archive);
        QCOMPARE(list(archive->directory()), (QStringList{u"skrót/"_s, u"skrót/notatka.md"_s}));
        QVERIFY(QFile::remove(path));
    }

    void unsupportedFormat()
    {
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(m_folder, u"rar"_s);
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).toString(), QString());
        QVERIFY(!finished.first().at(1).toString().isEmpty());
    }

    void missingFolder()
    {
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(m_dir.filePath(u"gone"_s), u"zip"_s);
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).toString(), QString());
        QVERIFY(finished.first().at(1).toString().contains(u"gone"_s));
    }

    void unreadableFile()
    {
        if (::geteuid() == 0) {
            QSKIP("root can read anything");
        }
        const QString secret = m_dir.filePath(u"tajne"_s);
        QVERIFY(QDir().mkpath(secret));
        QFile file(secret + u"/hasło.txt"_s);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.setPermissions(QFileDevice::Permissions()));
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(secret, u"tar.gz"_s);
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).toString(), QString());
        QVERIFY(finished.first().at(1).toString().contains(u"hasło.txt"_s));
        QCOMPARE(archivesLeft(), QStringList());
    }

    void unreadableSubfolder()
    {
        if (::geteuid() == 0) {
            QSKIP("root can read anything");
        }
        const QString locked = m_dir.filePath(u"zamknięte"_s);
        QVERIFY(QDir().mkpath(locked + u"/prywatne"_s));
        QVERIFY(QFile::setPermissions(locked + u"/prywatne"_s, QFileDevice::Permissions()));
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(locked, u"zip"_s);
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).toString(), QString());
        QVERIFY(finished.first().at(1).toString().contains(u"prywatne"_s));
        QCOMPARE(archivesLeft(), QStringList());
    }

    void zipTooLarge()
    {
        const QString movies = m_dir.filePath(u"filmy"_s);
        QVERIFY(QDir().mkpath(movies));
        QFile movie(movies + u"/film.mkv"_s);
        QVERIFY(movie.open(QIODevice::WriteOnly));
        struct stat info;
        // fits in 32 bits alone, but not with its headers
        if (!movie.resize(0xFFFFFFFF - 64) || ::stat(QFile::encodeName(movie.fileName()).constData(), &info) != 0 || info.st_blocks > 2048) {
            movie.remove();
            QSKIP("the file system cannot hold a sparse 4 GiB file");
        }
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(movies, u"zip"_s);
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).toString(), QString());
        QVERIFY(finished.first().at(1).toString().contains(u"4 GiB"_s));
        QCOMPARE(archivesLeft(), QStringList());
        QVERIFY(movie.remove());
    }

    void zipGrowsTooLarge()
    {
        const QString folder = m_dir.filePath(u"nagrania"_s);
        QVERIFY(QDir().mkpath(folder));
        const QStringList files = {folder + u"/a.bin"_s, folder + u"/b.bin"_s};
        for (const QString &file : files) {
            writeFile(file, randomBytes(8 * 1024 * 1024));
        }
        Archiver archiver;
        QSignalSpy progress(&archiver, &Packer::progress);
        QSignalSpy finished(&archiver, &Packer::finished);
        bool sparse = true;
        // the order is up to the file system, so both grow; the one being packed was counted already
        connect(&archiver, &Packer::progress, this, [&] {
            if (progress.size() == 1) {
                for (const QString &file : files) {
                    struct stat info;
                    sparse =
                        sparse && QFile::resize(file, 0xFFFFFFFF - 64) && ::stat(QFile::encodeName(file).constData(), &info) == 0 && info.st_blocks < 64 * 1024;
                }
            }
        });
        archiver.pack(folder, u"zip"_s);
        QVERIFY(finished.wait(60000));
        if (!sparse) {
            QVERIFY(QDir(folder).removeRecursively());
            QSKIP("the file system cannot hold a sparse 4 GiB file");
        }
        QCOMPARE(finished.first().at(0).toString(), QString());
        QVERIFY2(finished.first().at(1).toString().contains(u"4 GiB"_s), qPrintable(finished.first().at(1).toString()));
        QCOMPARE(archivesLeft(), QStringList());
        QVERIFY(QDir(folder).removeRecursively());
    }

    void openOnlyInside()
    {
        const QString outside = m_dir.filePath(u"obok"_s);
        QVERIFY(QDir().mkpath(outside));
        writeFile(outside + u"/sekret.txt"_s, "sekret");
        QVERIFY(QDir().mkpath(m_dir.filePath(u"korzeń"_s)));
        const QString root = QFileInfo(m_dir.filePath(u"korzeń"_s)).canonicalFilePath();
        writeFile(root + u"/plik.txt"_s, "plik");
        QVERIFY(QFile::link(u"plik.txt"_s, root + u"/link"_s));
        // as if a folder was swapped for a link while it was being packed
        QVERIFY(QFile::link(outside, root + u"/podmieniony"_s));
        QVERIFY(::mkfifo(QFile::encodeName(root + u"/fifo"_s).constData(), 0600) == 0);

        struct stat info;
        QFile file(root + u"/plik.txt"_s);
        QVERIFY(openInside(root, file, info).isEmpty());
        QCOMPARE(file.readAll(), "plik");
        QCOMPARE(info.st_size, 4);
        QVERIFY(S_ISREG(info.st_mode));

        for (const QString &name : {u"link"_s, u"podmieniony/sekret.txt"_s, u"fifo"_s}) {
            QFile file(root + u'/' + name);
            QVERIFY2(!openInside(root, file, info).isEmpty(), qPrintable(name));
            QVERIFY(!file.isOpen());
        }
    }

    void nameNotUtf8()
    {
        const QString folder = m_dir.filePath(u"stary dysk"_s);
        QVERIFY(QDir().mkpath(folder + u"/b"_s));
        // QDirIterator stops at the bad name, so there are good names on both sides of it
        writeFile(folder + u"/a.txt"_s, "a");
        writeFile(folder + u"/b/c.txt"_s, "c");
        const int fd = ::open(QFile::encodeName(folder).append("/b\xff.txt").constData(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) {
            QSKIP("the file system does not take names that are not valid UTF-8");
        }
        ::close(fd);
        writeFile(folder + u"/d.txt"_s, "d");
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(folder, u"tar.gz"_s);
        QVERIFY(finished.wait());
        QCOMPARE(finished.first().at(0).toString(), QString());
        QVERIFY(finished.first().at(1).toString().contains(folder));
        QCOMPARE(archivesLeft(), QStringList());
    }

    void zipTooManyEntries()
    {
        const QString folder = m_dir.filePath(u"wiele"_s);
        QVERIFY(QDir().mkpath(folder));
        // with the folder itself, one entry more than a zip without zip64 can count
        for (int i = 0; i < 0xFFFF - 1; ++i) {
            writeFile(folder + u'/' + QString::number(i), {});
        }
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(folder, u"zip"_s);
        QVERIFY(finished.wait(60000));
        QCOMPARE(finished.first().at(0).toString(), QString());
        QVERIFY(!finished.first().at(1).toString().isEmpty());
        QCOMPARE(archivesLeft(), QStringList());

        archiver.pack(folder, u"tar.gz"_s);
        QVERIFY(finished.wait(60000));
        QCOMPARE(finished.last().at(1).toString(), QString());
        QVERIFY(QFile::remove(finished.last().at(0).toString()));
        QVERIFY(QDir(folder).removeRecursively());
    }

    void fileShrinks()
    {
        const QString folder = m_dir.filePath(u"log"_s);
        QVERIFY(QDir().mkpath(folder));
        const QString log = folder + u"/app.log"_s;
        writeFile(log, randomBytes(16 * 1024 * 1024));
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        connect(&archiver, &Packer::progress, this, [&log] {
            QFile::resize(log, 0);
        });
        archiver.pack(folder, u"tar.xz"_s);
        QVERIFY(finished.wait(30000));
        QCOMPARE(finished.first().at(0).toString(), QString());
        const QString error = finished.first().at(1).toString();
        QVERIFY(error.contains(log));
        // "Unknown error", in whatever language the test runs
        QVERIFY2(!error.contains(QFile().errorString()), qPrintable(error));
        QCOMPARE(archivesLeft(), QStringList());
    }

    void folderGrows()
    {
        const QString folder = m_dir.filePath(u"rośnie"_s);
        QVERIFY(QDir().mkpath(folder));
        const QByteArray part = randomBytes(4 * 1024 * 1024);
        writeFile(folder + u"/a.bin"_s, part);
        writeFile(folder + u"/b.bin"_s, part);
        Archiver archiver;
        QSignalSpy progress(&archiver, &Packer::progress);
        QSignalSpy finished(&archiver, &Packer::finished);
        // the file not being packed yet is packed with what it got meanwhile
        connect(&archiver, &Packer::progress, this, [&] {
            if (progress.size() == 1) {
                writeFile(folder + u"/a.bin"_s, part, QIODevice::Append);
                writeFile(folder + u"/b.bin"_s, part, QIODevice::Append);
            }
        });
        archiver.pack(folder, u"tar.xz"_s);
        QVERIFY(finished.wait(30000));
        QCOMPARE(finished.first().at(1).toString(), QString());
        QVERIFY(QFile::remove(finished.first().at(0).toString()));
        for (const QVariantList &report : progress) {
            QVERIFY2(report.at(0).toLongLong() <= report.at(1).toLongLong(),
                     qPrintable(u"%1 of %2"_s.arg(report.at(0).toLongLong()).arg(report.at(1).toLongLong())));
        }
    }

    void packHome()
    {
        // home holds the cache, where the archive is written while its folder is packed
        const QString home = QDir::homePath();
        QVERIFY(QDir().mkpath(home));
        writeFile(home + u"/plik.txt"_s, "plik");
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(home, u"tar.gz"_s);
        QVERIFY(finished.wait(30000));
        const QString path = finished.first().at(0).toString();
        QCOMPARE(finished.first().at(1).toString(), QString());
        const std::unique_ptr<KArchive> archive = open(path);
        QVERIFY(archive);
        QCOMPARE(list(archive->directory()), (QStringList{u"dom/"_s, u"dom/.qttest/"_s, u"dom/.qttest/cache/"_s, u"dom/plik.txt"_s}));
        QVERIFY(QFile::remove(path));
    }

    void cancel()
    {
        Archiver archiver;
        QSignalSpy progress(&archiver, &Packer::progress);
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(m_slow, u"tar.xz"_s);
        QVERIFY(progress.wait());
        // does not wait for the worker, which may be stuck on a folder that does not reply
        QElapsedTimer timer;
        timer.start();
        archiver.cancel();
        QVERIFY2(timer.elapsed() < 20, qPrintable(QString::number(timer.elapsed())));
        const qsizetype reported = progress.size();
        QVERIFY(!finished.wait(500));
        QCOMPARE(progress.size(), reported);
        QTRY_COMPARE(archivesLeft(), QStringList());

        // and it can pack again afterwards
        archiver.pack(m_folder, u"zip"_s);
        QVERIFY(finished.wait(30000));
        QVERIFY(QFile::remove(finished.first().at(0).toString()));
    }

    void cancelWhenPacked()
    {
        const QString folder = m_dir.filePath(u"mały"_s);
        QVERIFY(QDir().mkpath(folder));
        Archiver archiver;
        QSignalSpy finished(&archiver, &Packer::finished);
        archiver.pack(folder, u"zip"_s);
        // the archive is done, but finished() has not been emitted yet
        QThreadPool::globalInstance()->waitForDone();
        archiver.cancel();
        QVERIFY(!finished.wait(500));
        QCOMPARE(archivesLeft(), QStringList());
    }

    void destroyWhilePacking()
    {
        auto archiver = std::make_unique<Archiver>();
        QSignalSpy progress(archiver.get(), &Packer::progress);
        archiver->pack(m_slow, u"tar.xz"_s);
        QVERIFY(progress.wait());
        QElapsedTimer timer;
        timer.start();
        archiver.reset();
        QVERIFY2(timer.elapsed() < 20, qPrintable(QString::number(timer.elapsed())));
        QTRY_COMPARE(archivesLeft(), QStringList());
    }

    void removeLeftovers()
    {
        QVERIFY(QDir().mkpath(cache()));
        for (const QString &name : {u"plasma-taildrop-a1B2c3.tar.gz"_s, u"inne.txt"_s}) {
            writeFile(cache() + u'/' + name, {});
        }
        Archiver::removeLeftovers();
        QCOMPARE(archivesLeft(), QStringList{u"inne.txt"_s});
        QVERIFY(QFile::remove(cache() + u"/inne.txt"_s));
    }
};

QTEST_GUILESS_MAIN(ArchiverTest)
#include "archivertest.moc"
